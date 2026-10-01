#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlParserStatus>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "DataStreamParser.h"

// The C runtime headers (pulled in by the Qt headers above) define `signal`, `stdin`,
// `stdout`, and `stderr` as macros. The ported Quickshell API uses those names, so they
// have to be un-defined here or moc cannot parse the declarations.
#ifdef signal
#undef signal
#endif
#ifdef stdin
#undef stdin
#endif
#ifdef stdout
#undef stdout
#endif
#ifdef stderr
#undef stderr
#endif

namespace wqs {

class DataStreamParser;

/// Port of Quickshell's `Process`. Runs an external program and streams its output into
/// `stdout`/`stderr` parser objects, exactly as a Quickshell config expects.
class Process : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(QVariant processId READ processId NOTIFY processIdChanged)
    Q_PROPERTY(QStringList command READ command WRITE setCommand NOTIFY commandChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(QVariantMap environment READ environment WRITE setEnvironment NOTIFY environmentChanged)
    Q_PROPERTY(bool clearEnvironment READ clearEnvironment WRITE setClearEnvironment NOTIFY clearEnvironmentChanged)
    Q_PROPERTY(bool stdinEnabled READ stdinEnabled WRITE setStdinEnabled NOTIFY stdinEnabledChanged)
    Q_PROPERTY(wqs::DataStreamParser *stdout READ stdoutParser WRITE setStdoutParser NOTIFY stdoutChanged)
    Q_PROPERTY(wqs::DataStreamParser *stderr READ stderrParser WRITE setStderrParser NOTIFY stderrChanged)

public:
    enum ExitedStatus {
        Normal = 0,
        Crash = 1,
    };
    Q_ENUM(ExitedStatus)

    explicit Process(QObject *parent = nullptr);
    ~Process() override;

    bool isRunning() const { return m_running; }
    void setRunning(bool running);
    QVariant processId() const;
    QStringList command() const { return m_command; }
    void setCommand(const QStringList &command);
    QString workingDirectory() const { return m_workingDirectory; }
    void setWorkingDirectory(const QString &dir);
    QVariantMap environment() const { return m_environment; }
    void setEnvironment(const QVariantMap &environment);
    bool clearEnvironment() const { return m_clearEnvironment; }
    void setClearEnvironment(bool clear);
    bool stdinEnabled() const { return m_stdinEnabled; }
    void setStdinEnabled(bool enabled);
    DataStreamParser *stdoutParser() const { return m_stdoutParser; }
    void setStdoutParser(DataStreamParser *parser);
    DataStreamParser *stderrParser() const { return m_stderrParser; }
    void setStderrParser(DataStreamParser *parser);

    void classBegin() override;
    void componentComplete() override;

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool startDetached();
    Q_INVOKABLE void write(const QByteArray &data);
    Q_INVOKABLE void closeStdin();
    Q_INVOKABLE void signal(int sig);
    Q_INVOKABLE void kill();
    Q_INVOKABLE void terminate();

signals:
    void runningChanged();
    void processIdChanged();
    void commandChanged();
    void workingDirectoryChanged();
    void environmentChanged();
    void clearEnvironmentChanged();
    void stdinEnabledChanged();
    void stdoutChanged();
    void stderrChanged();
    void started();
    void exited(int exitCode, Process::ExitedStatus status);
    void errorOccurred(QProcess::ProcessError error);

private:
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onErrorOccurred(QProcess::ProcessError error);
    void applyEnvironment();

    QProcess m_process;
    QStringList m_command;
    QString m_workingDirectory;
    QVariantMap m_environment;
    bool m_clearEnvironment = false;
    bool m_stdinEnabled = false;
    bool m_running = false;
    bool m_pendingStart = false;
    DataStreamParser *m_stdoutParser = nullptr;
    DataStreamParser *m_stderrParser = nullptr;
};

} // namespace wqs
