#include "Shell.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>

Shell::Shell(QObject *parent)
    : QObject(parent)
{
    refreshScreens();

    // Quickshell rederives this on hot-reload; wqs keeps the list live so a monitor is
    // added or removed, the next query sees it.
    if (auto *app = qobject_cast<QGuiApplication *>(QGuiApplication::instance())) {
        connect(app, &QGuiApplication::screenAdded, this, &Shell::refreshScreens);
        connect(app, &QGuiApplication::screenRemoved, this, &Shell::refreshScreens);
        connect(app, &QGuiApplication::primaryScreenChanged, this, &Shell::refreshScreens);
    }
}

Shell::~Shell() = default;

QStringList Shell::screens() const
{
    return m_screens;
}

QString Shell::configPath() const
{
    // Mirrors Quickshell's ~/.config/quickshell/ convention, on Windows that place is
    // %USERPROFILE%\.config\wqs, next to where wsddm keeps its config.
    return QDir::home().filePath(QStringLiteral(".config/wqs"));
}

QString Shell::dataPath() const
{
    return QDir::home().filePath(QStringLiteral(".local/share/wqs"));
}

QString Shell::logPath() const
{
    return QDir::home().filePath(QStringLiteral(".config/wqs/wqs.log"));
}

Shell::Source Shell::resolveSource(const QString &pathOpt, const QString &configOpt,
                                   QString *error)
{
    const QString root = QDir::home().filePath(QStringLiteral(".config/wqs"));

    if (!pathOpt.isEmpty()) {
        const QFileInfo info(pathOpt);
        const QString file = info.isDir()
            ? QDir(info.filePath()).filePath(QStringLiteral("shell.qml"))
            : pathOpt;
        if (!QFileInfo::exists(file)) {
            if (error)
                *error = QStringLiteral("config path not found: %1").arg(file);
            return {};
        }
        return {QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath()), false};
    }

    if (!configOpt.isEmpty()) {
        const QString file = QDir(root).filePath(configOpt + QStringLiteral("/shell.qml"));
        if (!QFileInfo::exists(file)) {
            if (error)
                *error = QStringLiteral("config '%1' not found in %2").arg(configOpt, root);
            return {};
        }
        return {QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath()), false};
    }

    // Quickshell convention: <config>/shell.qml is the config, otherwise a named
    // <config>/default/shell.qml. Fall back to the shell bundled into the executable.
    for (const QString &candidate :
         {QDir(root).filePath(QStringLiteral("shell.qml")),
          QDir(root).filePath(QStringLiteral("default/shell.qml"))}) {
        if (QFileInfo::exists(candidate))
            return {QUrl::fromLocalFile(QFileInfo(candidate).absoluteFilePath()), false};
    }

    return {QUrl(QStringLiteral("qrc:/qt/qml/wqs/qml/shell.qml")), true};
}

void Shell::refreshScreens()
{
    const QList<QScreen *> all = QGuiApplication::screens();

    QStringList names;
    QScreen *primary = QGuiApplication::primaryScreen();
    if (primary)
        names << primary->name();

    for (QScreen *screen : all) {
        if (!names.contains(screen->name()))
            names << screen->name();
    }

    if (names != m_screens) {
        m_screens = names;
        emit screensChanged();
    }
}