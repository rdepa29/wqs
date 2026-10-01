#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

/// Metadata for one running `wqs` process, persisted so `wqs list` / `wqs kill` /
/// `wqs ipc` can find it. Quickshell scopes instances to a display and talks to them over
/// an XDG runtime socket; Windows has neither, so wqs keys instances by process id and
/// talks to them over a QLocalServer named pipe (`wqs-<pid>`).
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

    /// Every registry entry that still answers a ping, with dead entries pruned.
    static QList<InstanceInfo> running();

    /// Sends one JSON command and waits for one reply. Returns false on any failure and
    /// sets `error` when non-null.
    static bool send(const InstanceInfo &info, const QJsonObject &command, QJsonObject *reply,
                     QString *error, int timeoutMs = 5000);
};
