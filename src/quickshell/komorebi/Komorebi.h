#pragma once

#include "KomorebiTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QLocalServer;
class QLocalSocket;
class QProcess;
class QTimer;

namespace wqs {

// Window manager state, fed by the komorebi event pipe
class Komorebi : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY workspacesChanged)
    Q_PROPERTY(QVariantList toplevels READ toplevels NOTIFY toplevelsChanged)
    Q_PROPERTY(KomorebiMonitor *focusedMonitor READ focusedMonitor NOTIFY focusedMonitorChanged)
    Q_PROPERTY(KomorebiWorkspace *focusedWorkspace READ focusedWorkspace NOTIFY focusedWorkspaceChanged)
    Q_PROPERTY(KomorebiToplevel *activeToplevel READ activeToplevel NOTIFY activeToplevelChanged)

public:
    explicit Komorebi(QObject *parent = nullptr);
    ~Komorebi() override;

    bool available() const { return m_available; }
    QVariantList monitors() const { return m_monitors; }
    QVariantList workspaces() const { return m_workspaces; }
    QVariantList toplevels() const { return m_toplevels; }
    KomorebiMonitor *focusedMonitor() const { return m_focusedMonitor; }
    KomorebiWorkspace *focusedWorkspace() const { return m_focusedWorkspace; }
    KomorebiToplevel *activeToplevel() const { return m_activeToplevel; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE KomorebiMonitor *monitorFor(QObject *screen) const;
    Q_INVOKABLE bool dispatch(const QString &command, const QStringList &args = {}) const;
    Q_INVOKABLE QString query(const QString &command, const QStringList &args = {}) const;

signals:
    void availableChanged();
    void monitorsChanged();
    void workspacesChanged();
    void toplevelsChanged();
    void focusedMonitorChanged();
    void focusedWorkspaceChanged();
    void activeToplevelChanged();
    void stateChanged();

private:
    void startSubscription();
    void launchSubscriber();
    void onNewConnection();
    void onReadyRead();
    void applyState(const QJsonObject &state);
    void rebuild();
    void setAvailable(bool available);
    void clearValueObjects();
    QByteArray run(const QStringList &args, int timeoutMs) const;

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_socket = nullptr;
    QProcess *m_process = nullptr;
    QTimer *m_debounce = nullptr;
    QString m_pipeName;
    QByteArray m_buffer;
    QByteArray m_pending;

    QVariantList m_monitors;
    QVariantList m_workspaces;
    QVariantList m_toplevels;
    KomorebiMonitor *m_focusedMonitor = nullptr;
    KomorebiWorkspace *m_focusedWorkspace = nullptr;
    KomorebiToplevel *m_activeToplevel = nullptr;
    bool m_available = false;
};

} // namespace wqs