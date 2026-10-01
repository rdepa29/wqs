#include "Quickshell.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScreen>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

#include "Screen.h"

namespace wqs {

QString Quickshell::s_shellDir;

Quickshell::Quickshell(QObject *parent)
    : QObject(parent)
{
    refreshScreens();
    m_workingDirectory = QDir::currentPath();

    if (auto *app = qobject_cast<QGuiApplication *>(QGuiApplication::instance())) {
        connect(app, &QGuiApplication::screenAdded, this, &Quickshell::refreshScreens);
        connect(app, &QGuiApplication::screenRemoved, this, &Quickshell::refreshScreens);
        connect(app, &QGuiApplication::primaryScreenChanged, this, &Quickshell::refreshScreens);
    }
    if (auto *clipboard = QGuiApplication::clipboard())
        connect(clipboard, &QClipboard::dataChanged, this, &Quickshell::clipboardTextChanged);
}

Quickshell::~Quickshell() = default;

Quickshell *Quickshell::create(QQmlEngine *, QJSEngine *)
{
    static Quickshell *instance = new Quickshell();
    return instance;
}

Quickshell *Quickshell::instance()
{
    return create(nullptr, nullptr);
}

void Quickshell::setShellDir(const QString &dir)
{
    s_shellDir = dir;
    if (auto *self = instance())
        emit self->shellDirChanged();
}

QString Quickshell::cacheDir() const
{
    return QDir::home().filePath(QStringLiteral(".cache/wqs"));
}

QString Quickshell::dataDir() const
{
    return QDir::home().filePath(QStringLiteral(".local/share/wqs"));
}

QString Quickshell::stateDir() const
{
    return QDir::home().filePath(QStringLiteral(".local/state/wqs"));
}

QString Quickshell::workingDirectory() const
{
    return m_workingDirectory;
}

void Quickshell::setWorkingDirectory(const QString &dir)
{
    if (m_workingDirectory == dir)
        return;
    m_workingDirectory = dir;
    emit workingDirectoryChanged();
}

void Quickshell::setWatchFiles(bool watch)
{
    if (m_watchFiles == watch)
        return;
    m_watchFiles = watch;
    emit watchFilesChanged();
}

qint32 Quickshell::processId() const
{
    return static_cast<qint32>(QCoreApplication::applicationPid());
}

QString Quickshell::clipboardText() const
{
    if (auto *clipboard = QGuiApplication::clipboard())
        return clipboard->text();
    return {};
}

void Quickshell::setClipboardText(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

QString Quickshell::cachePath(const QString &path) const
{
    return QDir(cacheDir()).filePath(path);
}

QString Quickshell::configPath(const QString &path) const
{
    return QDir(shellDir()).filePath(path);
}

QString Quickshell::dataPath(const QString &path) const
{
    return QDir(dataDir()).filePath(path);
}

QString Quickshell::statePath(const QString &path) const
{
    return QDir(stateDir()).filePath(path);
}

QString Quickshell::shellPath(const QString &path) const
{
    return QDir(shellDir()).filePath(path);
}

QVariant Quickshell::env(const QString &variable) const
{
    static const QString nullValue;
    if (!qEnvironmentVariableIsSet(variable.toUtf8().constData()))
        return QVariant();
    return qEnvironmentVariable(variable.toUtf8().constData());
}

void Quickshell::execDetached(const QVariant &context) const
{
    if (context.canConvert<QVariantList>() && !context.canConvert<QVariantMap>()) {
        const QVariantList list = context.toList();
        if (list.isEmpty())
            return;
        QStringList args;
        for (int i = 1; i < list.size(); ++i)
            args << list.at(i).toString();
        QProcess::startDetached(list.first().toString(), args);
        return;
    }
    if (context.canConvert<QVariantMap>()) {
        const QVariantMap map = context.toMap();
        const QVariantList list = map.value(QStringLiteral("command")).toList();
        if (list.isEmpty())
            return;
        QStringList args;
        for (int i = 1; i < list.size(); ++i)
            args << list.at(i).toString();
        const QString workingDir = map.value(QStringLiteral("workingDirectory")).toString();
        QProcess::startDetached(list.first().toString(), args, workingDir);
        return;
    }
    const QString command = context.toString();
    if (!command.isEmpty())
        QProcess::startDetached(command);
}

bool Quickshell::hasVersion(int major, int minor) const
{
    // Quickshell 0.3 API
    constexpr int kMajor = 0;
    constexpr int kMinor = 3;
    if (major != kMajor)
        return major < kMajor;
    return minor <= kMinor;
}

bool Quickshell::hasQtVersion(int major, int minor) const
{
    if (major != QT_VERSION_MAJOR)
        return major < QT_VERSION_MAJOR;
    return minor <= QT_VERSION_MINOR;
}

bool Quickshell::hasThemeIcon(const QString &) const
{
    return false;
}

QString Quickshell::iconPath(const QString &) const
{
    return {};
}

void Quickshell::inhibitReloadPopup() const
{
}

void Quickshell::reload(bool)
{
    // hot reload TBD; still emit so configs waiting on it keep going
    emit reloadCompleted();
}

void Quickshell::refreshScreens()
{
    const QList<QScreen *> all = QGuiApplication::screens();

    QList<QuickshellScreenInfo *> next;
    next.reserve(all.size());
    QScreen *primary = QGuiApplication::primaryScreen();
    if (primary)
        next << new QuickshellScreenInfo(primary, this);

    for (QScreen *screen : all) {
        if (screen == primary)
            continue;
        next << new QuickshellScreenInfo(screen, this);
    }

    if (!m_screens.isEmpty()) {
        for (QuickshellScreenInfo *old : m_screens)
            old->deleteLater();
    }

    m_screens = next;
    emit screensChanged();
}

} // namespace wqs
