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

    // also mirror to a console if one is attached
    QTextStream(stderr) << message << '\n';
}

} // namespace

int main(int argc, char *argv[])
{
    QStringList args;
    args.reserve(qMax(0, argc - 1));
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    // internal marker on the detached process; stripped before parsing
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

    // client subcommands never start a shell
    if (Cli::isClientCommand(command.kind)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("wqs"));
        app.setApplicationVersion(QStringLiteral(WQS_VERSION));
        return Cli::execute(command);
    }

    // resolve shell.qml before detaching so -n can still report a duplicate
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

    // start the shell detached so the terminal prompt returns immediately
    if (!alreadyDetached && Cli::relaunchDetached(args))
        return 0;

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wqs"));
    app.setOrganizationName(QStringLiteral("wqs"));
    app.setApplicationVersion(QStringLiteral(WQS_VERSION));
    // closing one window shouldn't quit the whole shell
    app.setQuitOnLastWindowClosed(false);

    qInstallMessageHandler(messageHandler);

    // root scope of the shell
    Shell shell;

    // shellDir = folder holding the entry shell.qml
    const QString shellDir = source.isResource
        ? QDir::home().filePath(QStringLiteral(".config/wqs"))
        : QFileInfo(entry).absolutePath();
    wqs::Quickshell::setShellDir(shellDir);

    QQmlApplicationEngine engine;

    // control channel for list/kill/ipc
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
