#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace Cli {

// internal marker on the detached shell process
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

Command parse(const QStringList &args);

bool isClientCommand(Kind kind);

int execute(const Command &command);

// relaunch detached so the shell runs without a console; true if a child was started
bool relaunchDetached(const QStringList &args);

void printVersion();
void printHelp();

} // namespace Cli
