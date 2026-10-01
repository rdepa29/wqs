#pragma once

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <functional>

#include "Instance.h"

class QLocalSocket;

// named-pipe control channel: list/kill/ipc, one per shell process
class IpcServer : public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<QVariant(const QVariantList &args, QString *error)>;

    explicit IpcServer(const QString &configPath, QObject *parent = nullptr);
    ~IpcServer() override;

    // the process's single server, so IpcHandlers can self-register
    static IpcServer *instance();

    bool listen();
    QString errorString() const { return m_server.errorString(); }

    void registerHandler(const QString &target, const QString &name, Handler handler);
    void unregisterHandler(const QString &target, const QString &name);

signals:
    void quitRequested();

private:
    void onNewConnection();
    void handleCommand(QLocalSocket *client, const QJsonObject &command);

    static void reply(QLocalSocket *client, const QJsonObject &payload);

    QLocalServer m_server;
    InstanceInfo m_info;
    QHash<QString, QHash<QString, Handler>> m_handlers;
};
