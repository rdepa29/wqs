#include "ProcessObject.h"

#include <QProcessEnvironment>
#include <QVariant>

#include "DataStreamParser.h"

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

Process::Process(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::started, this, &Process::onStarted);
    connect(&m_process, &QProcess::finished, this, &Process::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &Process::onErrorOccurred);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (m_stdoutParser)
            m_stdoutParser->parse(m_process.readAllStandardOutput());
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        if (m_stderrParser)
            m_stderrParser->parse(m_process.readAllStandardError());
    });
    connect(&m_process, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        const bool running = state == QProcess::Running;
        if (m_running != running) {
            m_running = running;
            emit runningChanged();
            emit processIdChanged();
        }
    });
}

Process::~Process()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QVariant Process::processId() const
{
    if (!m_running)
        return QVariant();
    return QVariant::fromValue(m_process.processId());
}

void Process::setRunning(bool running)
{
    if (m_running == running)
        return;
    if (running) {
        // `running: true` can come before `command`; retry in componentComplete
        m_pendingStart = !start();
    } else {
        m_pendingStart = false;
        if (m_process.state() != QProcess::NotRunning)
            m_process.terminate();
    }
}

void Process::classBegin() {}

void Process::componentComplete()
{
    if (m_pendingStart && !m_running) {
        m_pendingStart = false;
        start();
    }
}

void Process::setCommand(const QStringList &command)
{
    if (m_command == command)
        return;
    m_command = command;
    emit commandChanged();
}

void Process::setWorkingDirectory(const QString &dir)
{
    if (m_workingDirectory == dir)
        return;
    m_workingDirectory = dir;
    emit workingDirectoryChanged();
}

void Process::setEnvironment(const QVariantMap &environment)
{
    if (m_environment == environment)
        return;
    m_environment = environment;
    emit environmentChanged();
}

void Process::setClearEnvironment(bool clear)
{
    if (m_clearEnvironment == clear)
        return;
    m_clearEnvironment = clear;
    emit clearEnvironmentChanged();
}

void Process::setStdinEnabled(bool enabled)
{
    if (m_stdinEnabled == enabled)
        return;
    m_stdinEnabled = enabled;
    emit stdinEnabledChanged();
}

void Process::setStdoutParser(DataStreamParser *parser)
{
    if (m_stdoutParser == parser)
        return;
    m_stdoutParser = parser;
    emit stdoutChanged();
}

void Process::setStderrParser(DataStreamParser *parser)
{
    if (m_stderrParser == parser)
        return;
    m_stderrParser = parser;
    emit stderrChanged();
}

void Process::applyEnvironment()
{
    QProcessEnvironment env = m_clearEnvironment ? QProcessEnvironment()
                                                 : QProcessEnvironment::systemEnvironment();
    for (auto it = m_environment.constBegin(); it != m_environment.constEnd(); ++it) {
        if (it.value().isNull())
            env.remove(it.key());
        else
            env.insert(it.key(), it.value().toString());
    }
    m_process.setProcessEnvironment(env);
}

bool Process::start()
{
    if (m_running || m_command.isEmpty())
        return false;

    m_process.setProgram(m_command.first());
    m_process.setArguments(m_command.mid(1));

    if (!m_workingDirectory.isEmpty())
        m_process.setWorkingDirectory(m_workingDirectory);
    else
        m_process.setWorkingDirectory(QString());

    applyEnvironment();

    if (!m_stdinEnabled)
        m_process.setStandardInputFile(QProcess::nullDevice());

    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    return true;
}

bool Process::startDetached()
{
    if (m_command.isEmpty())
        return false;
    return QProcess::startDetached(m_command.first(), m_command.mid(1),
                                   m_workingDirectory.isEmpty() ? QString()
                                                                : m_workingDirectory);
}

void Process::write(const QByteArray &data)
{
    if (m_process.state() == QProcess::Running)
        m_process.write(data);
}

void Process::closeStdin()
{
    m_process.closeWriteChannel();
}

void Process::signal(int sig)
{
    Q_UNUSED(sig)
    m_process.kill();
}

void Process::kill()
{
    m_process.kill();
}

void Process::terminate()
{
    m_process.terminate();
}

void Process::onStarted()
{
    emit started();
    emit processIdChanged();
}

void Process::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stdoutParser) {
        m_stdoutParser->parse(m_process.readAllStandardOutput());
        m_stdoutParser->finish();
    }
    if (m_stderrParser) {
        m_stderrParser->parse(m_process.readAllStandardError());
        m_stderrParser->finish();
    }

    m_running = false;
    emit runningChanged();
    emit processIdChanged();
    emit exited(exitCode, exitStatus == QProcess::CrashExit ? Crash : Normal);
}

void Process::onErrorOccurred(QProcess::ProcessError error)
{
    emit errorOccurred(error);
}

} // namespace wqs
