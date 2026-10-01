#include "Cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

#include "Instance.h"
#include "Shell.h"

#ifdef Q_OS_WIN
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <string>
#endif

namespace {

bool isVerboseFlag(const QString &arg)
{
    if (arg.size() < 2 || arg.at(0) != QLatin1Char('-'))
        return false;
    for (int i = 1; i < arg.size(); ++i) {
        if (arg.at(i) != QLatin1Char('v'))
            return false;
    }
    return true;
}

void printValue(const QJsonValue &value, QTextStream &out)
{
    if (value.isObject())
        out << QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    else if (value.isArray())
        out << QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    else if (value.isString())
        out << value.toString();
    else if (value.isBool())
        out << (value.toBool() ? "true" : "false");
    else if (value.isDouble())
        out << value.toDouble();
    else if (value.isNull())
        out << "null";
    out << '\n';
}

#ifdef Q_OS_WIN
// quote for CommandLineToArgvW so a relaunched wqs keeps its args intact
QString quoteWindowsArg(const QString &arg)
{
    if (!arg.isEmpty() && !arg.contains(QLatin1Char(' ')) && !arg.contains(QLatin1Char('\t'))
        && !arg.contains(QLatin1Char('"')))
        return arg;

    QString out;
    out.reserve(arg.size() + 2);
    out += QLatin1Char('"');
    int backslashes = 0;
    for (const QChar c : arg) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (c == QLatin1Char('"')) {
            out += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            out += QLatin1Char('"');
        } else {
            out += QString(backslashes, QLatin1Char('\\'));
            out += c;
        }
        backslashes = 0;
    }
    out += QString(backslashes * 2, QLatin1Char('\\'));
    out += QLatin1Char('"');
    return out;
}
#endif

int runList()
{
    const QList<InstanceInfo> instances = InstanceInfo::running();

    QTextStream out(stdout);
    if (instances.isEmpty()) {
        out << "no running wqs instances\n";
        return 0;
    }
    for (const InstanceInfo &info : instances)
        out << "pid " << info.pid << "  " << info.configPath << '\n';
    return 0;
}

int runKill()
{
    const QList<InstanceInfo> instances = InstanceInfo::running();

    QTextStream out(stdout);
    QTextStream err(stderr);
    if (instances.isEmpty()) {
        err << "wqs: no running instances\n";
        return 1;
    }

    int killed = 0;
    for (const InstanceInfo &info : instances) {
        QJsonObject reply;
        QString error;
        const QJsonObject quit{{QStringLiteral("command"), QStringLiteral("quit")}};
        if (InstanceInfo::send(info, quit, &reply, &error)) {
            out << "killed " << info.pid << '\n';
            ++killed;
        } else {
            err << "wqs: failed to kill " << info.pid << ": " << error << '\n';
        }
    }
    return killed > 0 ? 0 : 1;
}

int runIpc(const Cli::Command &command)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QList<InstanceInfo> instances = InstanceInfo::running();
    if (instances.isEmpty()) {
        err << "wqs: no running instances\n";
        return 1;
    }
    if (instances.size() > 1) {
        err << "wqs: multiple instances running; ipc is ambiguous\n";
        return 1;
    }

    QJsonArray args;
    for (const QVariant &arg : command.args)
        args.append(QJsonValue::fromVariant(arg));

    const QJsonObject request{
        {QStringLiteral("command"), QStringLiteral("ipc")},
        {QStringLiteral("target"), command.target},
        {QStringLiteral("handler"), command.handler},
        {QStringLiteral("args"), args},
    };

    QJsonObject reply;
    QString error;
    if (!InstanceInfo::send(instances.first(), request, &reply, &error)) {
        err << "wqs: " << error << '\n';
        return 1;
    }
    if (reply.value(QStringLiteral("status")).toString() != QLatin1String("ok")) {
        err << "wqs: " << reply.value(QStringLiteral("error")).toString() << '\n';
        return 1;
    }

    printValue(reply.value(QStringLiteral("data")), out);
    return 0;
}

int runLog(const Cli::Command &command)
{
    const QString path = QDir::home().filePath(QStringLiteral(".config/wqs/wqs.log"));

    QFile file(path);
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "wqs: no log at " << path << '\n';
        return 1;
    }

    out << QString::fromUtf8(file.readAll());
    out.flush();
    if (!command.follow)
        return 0;

    qint64 position = file.pos();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&file, &out, &position, path]() {
        if (QFileInfo(path).size() <= position)
            return;
        file.seek(position);
        out << QString::fromUtf8(file.readAll());
        out.flush();
        position = file.pos();
    });
    timer.start(250);

    QEventLoop loop;
    return loop.exec();
}

} // namespace

bool Cli::relaunchDetached(const QStringList &args)
{
#ifdef Q_OS_WIN
    wchar_t module[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;

    QString commandLine = quoteWindowsArg(QString::fromWCharArray(module, int(length)));
    for (const QString &arg : args)
        commandLine += QLatin1Char(' ') + quoteWindowsArg(arg);
    commandLine += QLatin1Char(' ') + quoteWindowsArg(QString::fromLatin1(kDetachedArg));
    std::wstring mutableCommandLine = commandLine.toStdWString();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL ok = CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, FALSE,
                                   DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr,
                                   &startupInfo, &processInfo);
    if (!ok)
        return false;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    Q_UNUSED(args)
    return false;
#endif
}

Cli::Command Cli::parse(const QStringList &args)
{
    Command cmd;

    int i = 0;
    for (; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QLatin1String("--")) {
            ++i;
            break;
        }
        if (arg.isEmpty() || arg.at(0) != QLatin1Char('-') || arg == QLatin1String("-"))
            break;

        if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
            cmd.kind = Kind::Help;
            return cmd;
        }
        if (arg == QLatin1String("-V") || arg == QLatin1String("--version")) {
            cmd.kind = Kind::Version;
            return cmd;
        }
        if (arg == QLatin1String("-n") || arg == QLatin1String("--no-duplicate")) {
            cmd.noDuplicate = true;
            continue;
        }
        if (arg == QLatin1String("-p") || arg == QLatin1String("--path")) {
            if (i + 1 >= args.size()) {
                cmd.kind = Kind::Error;
                cmd.error = QStringLiteral("option '%1' requires a value").arg(arg);
                return cmd;
            }
            cmd.path = args.at(++i);
            continue;
        }
        if (arg.startsWith(QLatin1String("--path="))) {
            cmd.path = arg.mid(int(sizeof("--path=")) - 1);
            continue;
        }
        if (arg == QLatin1String("-c") || arg == QLatin1String("--config")) {
            if (i + 1 >= args.size()) {
                cmd.kind = Kind::Error;
                cmd.error = QStringLiteral("option '%1' requires a value").arg(arg);
                return cmd;
            }
            cmd.config = args.at(++i);
            continue;
        }
        if (arg.startsWith(QLatin1String("--config="))) {
            cmd.config = arg.mid(int(sizeof("--config=")) - 1);
            continue;
        }
        if (arg == QLatin1String("--no-color") || arg == QLatin1String("--log-times")
            || arg == QLatin1String("--any-display")) {
            continue;
        }
        if (isVerboseFlag(arg)) {
            cmd.verbose += arg.size() - 1;
            continue;
        }

        cmd.kind = Kind::Error;
        cmd.error = QStringLiteral("unknown option '%1'").arg(arg);
        return cmd;
    }

    if (i >= args.size()) {
        cmd.kind = Kind::Shell;
        return cmd;
    }

    const QString sub = args.at(i++);
    if (sub == QLatin1String("shell")) {
        cmd.kind = Kind::Shell;
    } else if (sub == QLatin1String("list")) {
        cmd.kind = Kind::List;
    } else if (sub == QLatin1String("kill")) {
        cmd.kind = Kind::Kill;
    } else if (sub == QLatin1String("log")) {
        cmd.kind = Kind::Log;
        for (; i < args.size(); ++i) {
            if (args.at(i) == QLatin1String("-f") || args.at(i) == QLatin1String("--follow")) {
                cmd.follow = true;
            } else {
                cmd.kind = Kind::Error;
                cmd.error = QStringLiteral("unexpected argument '%1' for 'log'").arg(args.at(i));
                return cmd;
            }
        }
    } else if (sub == QLatin1String("ipc")) {
        cmd.kind = Kind::Ipc;
        if (i + 1 >= args.size() || args.at(i) != QLatin1String("call")) {
            cmd.kind = Kind::Error;
            cmd.error = QStringLiteral("usage: wqs ipc call <target> <handler> [args...]");
            return cmd;
        }
        i += 1;
        if (i + 1 >= args.size()) {
            cmd.kind = Kind::Error;
            cmd.error = QStringLiteral("usage: wqs ipc call <target> <handler> [args...]");
            return cmd;
        }
        cmd.target = args.at(i++);
        cmd.handler = args.at(i++);
        for (; i < args.size(); ++i)
            cmd.args << args.at(i);
    } else {
        cmd.kind = Kind::Error;
        cmd.error = QStringLiteral("unknown subcommand '%1'").arg(sub);
    }

    return cmd;
}

bool Cli::isClientCommand(Kind kind)
{
    return kind == Kind::List || kind == Kind::Kill || kind == Kind::Ipc || kind == Kind::Log;
}

int Cli::execute(const Command &command)
{
    switch (command.kind) {
    case Kind::List:
        return runList();
    case Kind::Kill:
        return runKill();
    case Kind::Ipc:
        return runIpc(command);
    case Kind::Log:
        return runLog(command);
    default:
        return 0;
    }
}

void Cli::printVersion()
{
    QTextStream(stdout) << "wqs " << WQS_VERSION << '\n';
}

void Cli::printHelp()
{
    QTextStream(stdout)
        << "wqs - a Quickshell port for Windows\n"
        << "\n"
        << "Usage:\n"
        << "  wqs [options] [subcommand]\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help             Print this help and exit.\n"
        << "  -V, --version          Print the version and exit.\n"
        << "  -n, --no-duplicate     Exit if an instance is already running.\n"
        << "  -v, --verbose          Increase log verbosity (repeatable).\n"
        << "  -p, --path <path>      Run the shell.qml at <path> (a file or a directory).\n"
        << "  -c, --config <name>    Run <config-dir>/<name>/shell.qml.\n"
        << "\n"
        << "Subcommands:\n"
        << "  (none)                 Run the shell.\n"
        << "  list                   List running wqs instances.\n"
        << "  kill                   Kill running wqs instances.\n"
        << "  ipc call <target> <handler> [args...]\n"
        << "                         Call an IPC handler on a running instance.\n"
        << "  log [-f]               Print the wqs log; -f follows it.\n";
}
