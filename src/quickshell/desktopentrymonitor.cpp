#include "desktopentrymonitor.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include "desktopentry.h"

namespace {
void addPathAndParents(QFileSystemWatcher &watcher, const QString &path)
{
    watcher.addPath(path);

    auto p = QFileInfo(path).absolutePath();
    while (!p.isEmpty()) {
        watcher.addPath(p);
        const auto parent = QFileInfo(p).dir().absolutePath();
        if (parent == p)
            break;
        p = parent;
    }
}
} // namespace

DesktopEntryMonitor::DesktopEntryMonitor(QObject *parent) : QObject(parent)
{
    this->debounceTimer.setSingleShot(true);
    this->debounceTimer.setInterval(100);

    QObject::connect(
        &this->watcher,
        &QFileSystemWatcher::directoryChanged,
        this,
        &DesktopEntryMonitor::onDirectoryChanged
    );
    QObject::connect(
        &this->debounceTimer,
        &QTimer::timeout,
        this,
        &DesktopEntryMonitor::processChanges
    );

    this->startMonitoring();
}

void DesktopEntryMonitor::startMonitoring()
{
    for (const auto &path: DesktopEntryManager::desktopPaths()) {
        if (!QDir(path).exists())
            continue;
        addPathAndParents(this->watcher, path);
        this->scanAndWatch(path);
    }
}

void DesktopEntryMonitor::scanAndWatch(const QString &dirPath)
{
    // A watched directory that is later removed leaves a stale watcher path behind.
    if (!QDir(dirPath).exists()) {
        this->watcher.removePath(dirPath);
        return;
    }

    this->watcher.addPath(dirPath);

    // The Start Menu nests shortcuts in per-folder subdirectories (for example
    // "Administrative Tools"), so watching must recurse to see them.
    const auto subdirs =
        QDir(dirPath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &subdir: subdirs)
        this->scanAndWatch(subdir.absoluteFilePath());
}

void DesktopEntryMonitor::onDirectoryChanged(const QString & /*path*/)
{
    this->debounceTimer.start();
}

void DesktopEntryMonitor::processChanges() { emit this->desktopEntriesChanged(); }