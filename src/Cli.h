#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

/// Command-line front end for wqs, mirroring Quickshell's `qs` tool (invoked here as
/// `wqs`). `parse()` turns argv into a Command; main() then either dispatches a client
/// subcommand (list/kill/ipc/log) or starts the shell.
namespace Cli {

/// Internal argument that marks the detached shell process, so it does not try to
/// detach itself again. Not user-facing.
inline constexpr auto kDetachedArg = "--wqs-detached";

enum class Kind {
    Shell,
    List,
    Kill,
    Ipc,
    Log,
    Version,
    Help,
    Error,
};

struct Command
{
    Kind kind = Kind::Shell;
    bool noDuplicate = false;
    int verbose = 0;
    QString error;

    // config selection (-p/--path, -c/--config)
    QString path;
    QString config;

    // ipc call <target> <handler> [args...]
    QString target;
    QString handler;
    QVariantList args;

    // log
    bool follow = false;
};

/// Parses arguments, excluding argv[0].
Command parse(const QStringList &args);

/// True for subcommands handled entirely on the client, without starting a shell.
bool isClientCommand(Kind kind);

/// Runs a client subcommand and returns the process exit code.
int execute(const Command &command);

/// Re-launches this executable with the same arguments, detached from the console, so
/// the shell can run without a console window while the caller's prompt returns.
/// Returns true when a detached process was started (the caller should then exit);
/// false when there is no console to detach from or on non-Windows platforms.
bool relaunchDetached(const QStringList &args);

void printVersion();
void printHelp();

} // namespace Cli
