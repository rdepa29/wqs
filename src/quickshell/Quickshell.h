#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

#include "Screen.h"

class QQmlEngine;
class QJSEngine;

namespace wqs {

/// Port of the `Quickshell` QML singleton. Every shell config imports this (`import
/// Quickshell`) and reaches for paths, screens, and helpers through it. wqs keeps the
/// surface that makes sense on Windows: directories, screens, process id, version
/// gates, and detached execution. Wayland-only facilities are inert.
class Quickshell : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString stateDir READ stateDir CONSTANT)
    Q_PROPERTY(QString shellDir READ shellDir NOTIFY shellDirChanged)
    Q_PROPERTY(QString configDir READ shellDir NOTIFY shellDirChanged)
    Q_PROPERTY(QString shellRoot READ shellDir NOTIFY shellDirChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(bool watchFiles READ watchFiles WRITE setWatchFiles NOTIFY watchFilesChanged)
    Q_PROPERTY(QList<QuickshellScreenInfo *> screens READ screens NOTIFY screensChanged)
    Q_PROPERTY(qint32 processId READ processId CONSTANT)
    Q_PROPERTY(QString clipboardText READ clipboardText WRITE setClipboardText NOTIFY clipboardTextChanged)

public:
    explicit Quickshell(QObject *parent = nullptr);
    ~Quickshell() override;

    static Quickshell *create(QQmlEngine *engine, QJSEngine *jsEngine);
    static Quickshell *instance();

    /// The directory of the entry shell.qml, published by main() so `Quickshell.shellDir`
    /// (and shellPath()) resolve the way a Quickshell config expects.
    static void setShellDir(const QString &dir);

    QString cacheDir() const;
    QString dataDir() const;
    QString stateDir() const;
    QString shellDir() const { return s_shellDir; }
    QString workingDirectory() const;
    void setWorkingDirectory(const QString &dir);
    bool watchFiles() const { return m_watchFiles; }
    void setWatchFiles(bool watch);
    QList<QuickshellScreenInfo *> screens() const { return m_screens; }
    qint32 processId() const;
    QString clipboardText() const;
    void setClipboardText(const QString &text);

    Q_INVOKABLE QString cachePath(const QString &path) const;
    Q_INVOKABLE QString configPath(const QString &path) const;
    Q_INVOKABLE QString dataPath(const QString &path) const;
    Q_INVOKABLE QString statePath(const QString &path) const;
    Q_INVOKABLE QString shellPath(const QString &path) const;
    Q_INVOKABLE QVariant env(const QString &variable) const;
    Q_INVOKABLE void execDetached(const QVariant &context) const;
    Q_INVOKABLE bool hasVersion(int major, int minor) const;
    Q_INVOKABLE bool hasQtVersion(int major, int minor) const;
    Q_INVOKABLE bool hasThemeIcon(const QString &icon) const;
    Q_INVOKABLE QString iconPath(const QString &icon) const;
    Q_INVOKABLE void inhibitReloadPopup() const;
    Q_INVOKABLE void reload(bool hard = false);

signals:
    void shellDirChanged();
    void workingDirectoryChanged();
    void watchFilesChanged();
    void screensChanged();
    void clipboardTextChanged();
    void reloadCompleted();
    void reloadFailed(const QString &errorString);
    void lastWindowClosed();

private:
    void refreshScreens();

    static QString s_shellDir;

    QList<QuickshellScreenInfo *> m_screens;
    QString m_workingDirectory;
    bool m_watchFiles = true;
};

} // namespace wqs
