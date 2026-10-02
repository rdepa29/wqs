#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>

class DesktopEntryManager;

class DesktopEntryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DesktopEntryMonitor(QObject *parent = nullptr);
    ~DesktopEntryMonitor() override = default;
    DesktopEntryMonitor(const DesktopEntryMonitor &) = delete;
    DesktopEntryMonitor &operator=(const DesktopEntryMonitor &) = delete;
    DesktopEntryMonitor(DesktopEntryMonitor &&) = delete;
    DesktopEntryMonitor &operator=(DesktopEntryMonitor &&) = delete;

signals:
    void desktopEntriesChanged();

private slots:
    void onDirectoryChanged(const QString &path);
    void processChanges();

private:
    void startMonitoring();
    void scanAndWatch(const QString &dirPath);

    QFileSystemWatcher watcher;
    QTimer debounceTimer;
};