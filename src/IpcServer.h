#pragma once

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <functional>

#include "Instance.h"

class QLocalSocket;

/// Server side of the wqs control channel. One per running shell process. It listens on a
/// named pipe, answers `wqs list` / `wqs kill`, and routes `wqs ipc call` to a small
/// handler registry - the counterpart of a Quickshell `IpcHandler`. Built-in handlers
/// (the `wqs` target: `version`, `screens`) are registered from main.cpp.
class IpcServer : public QObject
{
    Q_OBJECT

public:
    /// `error` is set to a message on failure, otherwise left empty.
    using Handler = std::function<QVariant(const QVariantList &args, QString *error)>;

    explicit IpcServer(const QString &configPath, QObject *parent = nullptr);
    ~IpcServer() override;

    /// The single server for this process, so QML `IpcHandler`s can register themselves
    /// without a context property.
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
