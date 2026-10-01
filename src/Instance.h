#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// one running wqs process; keyed by pid, reachable over a `wqs-<pid>` named pipe
struct InstanceInfo
{
    qint64 pid = 0;
    QString socketName;
    QString configPath;

    QJsonObject toJson() const;
    static InstanceInfo fromJson(const QJsonObject &json);

    static QString stateDir();
    static QString registryDir();
    static QString socketForPid(qint64 pid);

    QString registryFile() const;
    bool writeToRegistry() const;
    void removeFromRegistry() const;

    // live entries only, dead ones pruned
    static QList<InstanceInfo> running();

    // send one JSON command, wait for one reply
    static bool send(const InstanceInfo &info, const QJsonObject &command, QJsonObject *reply,
                     QString *error, int timeoutMs = 5000);
};
