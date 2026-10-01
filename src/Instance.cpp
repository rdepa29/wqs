#include "Instance.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>

QJsonObject InstanceInfo::toJson() const
{
    return QJsonObject{
        {QStringLiteral("pid"), static_cast<double>(pid)},
        {QStringLiteral("socket"), socketName},
        {QStringLiteral("config"), configPath},
    };
}

InstanceInfo InstanceInfo::fromJson(const QJsonObject &json)
{
    InstanceInfo info;
    info.pid = static_cast<qint64>(json.value(QStringLiteral("pid")).toDouble());
    info.socketName = json.value(QStringLiteral("socket")).toString();
    info.configPath = json.value(QStringLiteral("config")).toString();
    return info;
}

QString InstanceInfo::stateDir()
{
    return QDir::home().filePath(QStringLiteral(".local/state/wqs"));
}

QString InstanceInfo::registryDir()
{
    return QDir(stateDir()).filePath(QStringLiteral("instances"));
}

QString InstanceInfo::socketForPid(qint64 pid)
{
    return QStringLiteral("wqs-%1").arg(pid);
}

QString InstanceInfo::registryFile() const
{
    return QDir(registryDir()).filePath(QStringLiteral("%1.json").arg(pid));
}

bool InstanceInfo::writeToRegistry() const
{
    QDir().mkpath(registryDir());

    QFile file(registryFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
    return true;
}

void InstanceInfo::removeFromRegistry() const
{
    QFile::remove(registryFile());
}

QList<InstanceInfo> InstanceInfo::running()
{
    QList<InstanceInfo> instances;

    QDir dir(registryDir());
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &name : files) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        const InstanceInfo info = fromJson(json);
        if (info.pid <= 0 || info.socketName.isEmpty())
            continue;

        QJsonObject reply;
        const QJsonObject ping{{QStringLiteral("command"), QStringLiteral("ping")}};
        if (send(info, ping, &reply, nullptr, 500))
            instances << info;
        else
            info.removeFromRegistry();
    }

    return instances;
}

bool InstanceInfo::send(const InstanceInfo &info, const QJsonObject &command, QJsonObject *reply,
                        QString *error, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(info.socketName);
    if (!socket.waitForConnected(timeoutMs)) {
        if (error)
            *error = socket.errorString();
        return false;
    }

    socket.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(timeoutMs)) {
        if (error)
            *error = socket.errorString();
        return false;
    }

    QByteArray data;
    // the reply can land together with the peer closing, so drain both before and
    // after a failed wait
    for (;;) {
        if (socket.bytesAvailable() > 0) {
            data += socket.readAll();
            if (data.contains('\n'))
                break;
        }
        if (socket.state() != QLocalSocket::ConnectedState) {
            data += socket.readAll();
            break;
        }
        if (!socket.waitForReadyRead(timeoutMs)) {
            data += socket.readAll();
            break;
        }
    }

    if (!data.contains('\n')) {
        if (error)
            *error = socket.errorString();
        return false;
    }

    const QJsonObject response =
        QJsonDocument::fromJson(data.left(data.indexOf('\n'))).object();
    if (reply)
        *reply = response;
    return true;
}
