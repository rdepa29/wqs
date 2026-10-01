#pragma once

#include <QList>
#include <QObject>
#include <QQmlListProperty>
#include <QtQmlIntegration/qqmlintegration.h>

namespace wqs {

/// Port of `QuickshellSettings`, reached through @@ShellRoot.settings. Quickshell uses it
/// for reload and watch behavior; wqs keeps the fields a config may set so it loads, and
/// wires up what maps to this port.
class QuickshellSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(QuickshellSettings)
    QML_UNCREATABLE("QuickshellSettings is provided by ShellRoot")

    Q_PROPERTY(bool watchFiles READ watchFiles WRITE setWatchFiles NOTIFY watchFilesChanged)
    Q_PROPERTY(bool reloadPopup READ reloadPopup WRITE setReloadPopup NOTIFY reloadPopupChanged)

public:
    explicit QuickshellSettings(QObject *parent = nullptr);

    bool watchFiles() const { return m_watchFiles; }
    void setWatchFiles(bool watch);
    bool reloadPopup() const { return m_reloadPopup; }
    void setReloadPopup(bool popup);

signals:
    void watchFilesChanged();
    void reloadPopupChanged();

private:
    bool m_watchFiles = true;
    bool m_reloadPopup = true;
};

/// Port of `Scope`: a non-visual container whose children behave as if declared in the
/// parent. Configs nest windows and singletons inside it.
class Scope : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQmlListProperty<QObject> children READ children)
    Q_CLASSINFO("DefaultProperty", "children")

public:
    explicit Scope(QObject *parent = nullptr);

    QQmlListProperty<QObject> children();

private:
    static void appendChild(QQmlListProperty<QObject> *prop, QObject *object);
    static qsizetype childCount(QQmlListProperty<QObject> *prop);
    static QObject *childAt(QQmlListProperty<QObject> *prop, qsizetype index);
    static void clearChildren(QQmlListProperty<QObject> *prop);

    QList<QObject *> m_children;
};

/// Port of `ShellRoot`, the optional root element of a shell config.
class ShellRoot : public Scope
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QuickshellSettings *settings READ settings CONSTANT)

public:
    explicit ShellRoot(QObject *parent = nullptr);

    QuickshellSettings *settings() const { return m_settings; }

private:
    QuickshellSettings *m_settings = nullptr;
};

} // namespace wqs
