#include "IpcServer.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocalSocket>

namespace {
IpcServer *g_instance = nullptr;
}

IpcServer *IpcServer::instance()
{
    return g_instance;
}

IpcServer::IpcServer(const QString &configPath, QObject *parent)
    : QObject(parent)
{
    g_instance = this;
    m_info.pid = QCoreApplication::applicationPid();
    m_info.socketName = InstanceInfo::socketForPid(m_info.pid);
    m_info.configPath = configPath;
}

IpcServer::~IpcServer()
{
    if (g_instance == this)
        g_instance = nullptr;
    m_server.close();
    m_info.removeFromRegistry();
}

bool IpcServer::listen()
{
    // A crashed predecessor may have left the pipe behind.
    QLocalServer::removeServer(m_info.socketName);
    if (!m_server.listen(m_info.socketName))
        return false;

    connect(&m_server, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);
    m_info.writeToRegistry();
    return true;
}

void IpcServer::registerHandler(const QString &target, const QString &name, Handler handler)
{
    m_handlers[target][name] = std::move(handler);
}

void IpcServer::unregisterHandler(const QString &target, const QString &name)
{
    auto targetHandlers = m_handlers.find(target);
    if (targetHandlers == m_handlers.end())
        return;
    targetHandlers->remove(name);
    if (targetHandlers->isEmpty())
        m_handlers.erase(targetHandlers);
}

void IpcServer::onNewConnection()
{
    while (QLocalSocket *client = m_server.nextPendingConnection()) {
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            const QByteArray data = client->readAll();
            const int newline = data.indexOf('\n');
            if (newline < 0)
                return; // wait for the rest of the line

            handleCommand(client, QJsonDocument::fromJson(data.left(newline)).object());
        });
        connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
}

void IpcServer::handleCommand(QLocalSocket *client, const QJsonObject &command)
{
    const QString name = command.value(QStringLiteral("command")).toString();

    if (name == QLatin1String("ping")) {
        reply(client, {{QStringLiteral("status"), QStringLiteral("ok")},
                       {QStringLiteral("data"), QStringLiteral("pong")}});
    } else if (name == QLatin1String("info")) {
        QJsonObject data = m_info.toJson();
        data.insert(QStringLiteral("version"), QCoreApplication::applicationVersion());
        reply(client, {{QStringLiteral("status"), QStringLiteral("ok")},
                       {QStringLiteral("data"), data}});
    } else if (name == QLatin1String("quit")) {
        reply(client, {{QStringLiteral("status"), QStringLiteral("ok")}});
        emit quitRequested();
    } else if (name == QLatin1String("ipc")) {
        const QString target = command.value(QStringLiteral("target")).toString();
        const QString handlerName = command.value(QStringLiteral("handler")).toString();
        const QVariantList args = command.value(QStringLiteral("args")).toArray().toVariantList();

        const auto targetHandlers = m_handlers.constFind(target);
        if (targetHandlers == m_handlers.constEnd()
            || !targetHandlers->contains(handlerName)) {
            reply(client, {{QStringLiteral("status"), QStringLiteral("error")},
                           {QStringLiteral("error"),
                            QStringLiteral("no handler '%1' on target '%2'")
                                .arg(handlerName, target)}});
            return;
        }

        QString error;
        const QVariant result = targetHandlers->value(handlerName)(args, &error);
        if (!error.isEmpty())
            reply(client, {{QStringLiteral("status"), QStringLiteral("error")},
                           {QStringLiteral("error"), error}});
        else
            reply(client, {{QStringLiteral("status"), QStringLiteral("ok")},
                           {QStringLiteral("data"), QJsonValue::fromVariant(result)}});
    } else {
        reply(client, {{QStringLiteral("status"), QStringLiteral("error")},
                       {QStringLiteral("error"),
                        QStringLiteral("unknown command '%1'").arg(name)}});
    }
}

void IpcServer::reply(QLocalSocket *client, const QJsonObject &payload)
{
    client->write(QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n');
    client->flush();
    client->waitForBytesWritten(500);
    client->disconnectFromServer();
}
