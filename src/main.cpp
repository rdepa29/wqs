#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTextStream>

#include "Cli.h"
#include "Instance.h"
#include "IpcServer.h"
#include "Shell.h"
#include "quickshell/Quickshell.h"

namespace {

// Minimal file logger. Quickshell writes to its runtime dir; wqs uses a stable
// %USERPROFILE%\.config\wqs\wqs.log so issues show up in one place across runs. Logging
// is best-effort: an unwritable directory simply means no log line.
void logLine(const QString &line)
{
    QDir dir(QDir::home().filePath(QStringLiteral(".config/wqs")));
    dir.mkpath(QStringLiteral("."));

    QFile file(dir.filePath(QStringLiteral("wqs.log")));
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' ' << line << '\n';
    file.close();
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    Q_UNUSED(type)
    logLine(message);

    // Also mirror to the console when one is attached, so a shell started from a
    // terminal shows the same diagnostics that land in the log file.
    QTextStream(stderr) << message << '\n';
}

} // namespace

int main(int argc, char *argv[])
{
    QStringList args;
    args.reserve(qMax(0, argc - 1));
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    // Internal marker set on the detached shell process; stripped before parsing.
    const bool alreadyDetached = args.removeAll(QString::fromLatin1(Cli::kDetachedArg)) > 0;

    const Cli::Command command = Cli::parse(args);

    if (command.kind == Cli::Kind::Error) {
        QTextStream(stderr) << "wqs: " << command.error << "\n\n";
        Cli::printHelp();
        return 2;
    }
    if (command.kind == Cli::Kind::Help) {
        Cli::printHelp();
        return 0;
    }
    if (command.kind == Cli::Kind::Version) {
        Cli::printVersion();
        return 0;
    }

    // Subcommands talk to a running shell; they never start one themselves.
    if (Cli::isClientCommand(command.kind)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("wqs"));
        app.setApplicationVersion(QStringLiteral(WQS_VERSION));
        return Cli::execute(command);
    }

    // Resolve which shell.qml to run before detaching, so `-n` can still report a duplicate
    // instance to the terminal that launched wqs. Quickshell's config order is ported:
    // -p/--path or -c/--config, with WQS_CONFIG_PATH / WQS_CONFIG_NAME as fallbacks.
    const QString pathOpt = !command.path.isEmpty()
        ? command.path
        : qEnvironmentVariable("WQS_CONFIG_PATH");
    const QString configOpt = !command.config.isEmpty()
        ? command.config
        : qEnvironmentVariable("WQS_CONFIG_NAME");

    QString resolveError;
    const Shell::Source source = Shell::resolveSource(pathOpt, configOpt, &resolveError);
    if (source.url.isEmpty()) {
        QTextStream(stderr) << "wqs: " << resolveError << '\n';
        return 1;
    }
    const QString entry =
        source.isResource ? source.url.toString() : source.url.toLocalFile();

    if (command.noDuplicate) {
        for (const InstanceInfo &info : InstanceInfo::running()) {
            if (info.configPath == entry) {
                QTextStream(stderr) << "wqs: another instance is already running (pid "
                                    << info.pid << ")\n";
                return 0;
            }
        }
    }

    // The shell is a long-running windowed process. Start it detached so launching it from
    // a terminal returns the prompt immediately and shows no console window, while the
    // client subcommands above keep the terminal they were run from.
    if (!alreadyDetached && Cli::relaunchDetached(args))
        return 0;

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wqs"));
    app.setOrganizationName(QStringLiteral("wqs"));
    app.setApplicationVersion(QStringLiteral(WQS_VERSION));
    // A shell owns several windows (panels, floating windows, popups). Closing one must
    // not tear the whole process down.
    app.setQuitOnLastWindowClosed(false);

    qInstallMessageHandler(messageHandler);

    // ShellRoot is the owning scope of the whole shell, mirroring Quickshell's ShellRoot.
    // Everything the shell needs (paths, screens, version) hangs off it.
    Shell shell;

    // Quickshell.shellDir points at the folder holding the entry shell.qml, matching the
    // directory a Quickshell config would see.
    const QString shellDir = source.isResource
        ? QDir::home().filePath(QStringLiteral(".config/wqs"))
        : QFileInfo(entry).absolutePath();
    wqs::Quickshell::setShellDir(shellDir);

    QQmlApplicationEngine engine;

    // The control channel behind `wqs list` / `wqs kill` / `wqs ipc`. The built-in `wqs`
    // target mirrors the handful of Quickshell IPC handlers that make sense here.
    IpcServer ipc(entry);
    ipc.registerHandler(QStringLiteral("wqs"), QStringLiteral("version"),
                        [](const QVariantList &, QString *) -> QVariant {
                            return QStringLiteral(WQS_VERSION);
                        });
    ipc.registerHandler(QStringLiteral("wqs"), QStringLiteral("screens"),
                        [&shell](const QVariantList &, QString *) -> QVariant {
                            return shell.screens();
                        });
    if (!ipc.listen())
        qWarning().noquote() << "wqs: IPC server failed to start:" << ipc.errorString();
    QObject::connect(&ipc, &IpcServer::quitRequested, &app, &QCoreApplication::quit);

    engine.load(source.url);
    if (engine.rootObjects().isEmpty()) {
        qCritical().noquote() << "wqs: failed to load" << entry << "(details above in the log)";
        return 1;
    }

    return app.exec();
}
