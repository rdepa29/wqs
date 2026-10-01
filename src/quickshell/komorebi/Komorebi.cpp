#include "Komorebi.h"

#include "KomorebiTypes.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>

namespace wqs {

namespace {
const char *kExecutable = "komorebic";
const int kDebounceMs = 120;
const int kRetryMs = 1000;
}

Komorebi::Komorebi(QObject *parent)
    : QObject(parent)
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &Komorebi::rebuild);

    // bootstrap off the event loop so the first QML access is not blocked
    QTimer::singleShot(0, this, [this] {
        startSubscription();
        refresh();
    });
}

Komorebi::~Komorebi()
{
    if (m_socket)
        m_socket->disconnectFromServer();

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
}

// komorebic connects to us, so the pipe must be listening before it is launched
void Komorebi::startSubscription()
{
    if (!m_server) {
        m_pipeName = QStringLiteral("wqs-komorebi-%1").arg(QCoreApplication::applicationPid());
        QLocalServer::removeServer(m_pipeName);

        m_server = new QLocalServer(this);
        if (!m_server->listen(m_pipeName)) {
            qWarning("wqs: komorebi pipe listen failed: %s", qPrintable(m_server->errorString()));
            return;
        }

        connect(m_server, &QLocalServer::newConnection, this, &Komorebi::onNewConnection);
    }

    launchSubscriber();
}

void Komorebi::launchSubscriber()
{
    if (!m_server || !m_server->isListening())
        return;

    if (m_process && m_process->state() != QProcess::NotRunning)
        return;

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished, this, [this] { QTimer::singleShot(kRetryMs, this, &Komorebi::launchSubscriber); });
    }

    m_process->start(QString::fromLatin1(kExecutable),
                     {QStringLiteral("subscribe-pipe"), m_pipeName});
}

void Komorebi::onNewConnection()
{
    QLocalSocket *socket = m_server->nextPendingConnection();
    if (!socket)
        return;

    if (m_socket) {
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
    }

    m_socket = socket;
    m_buffer.clear();

    connect(m_socket, &QLocalSocket::readyRead, this, &Komorebi::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, [this] {
        QTimer::singleShot(kRetryMs, this, &Komorebi::launchSubscriber);
    });
}

// every event repeats the whole state, so only the newest complete line matters
void Komorebi::onReadyRead()
{
    if (!m_socket)
        return;

    m_buffer.append(m_socket->readAll());

    while (true) {
        const qsizetype brk = m_buffer.indexOf('\n');
        if (brk < 0)
            break;

        const QByteArray line = m_buffer.left(brk);
        m_buffer.remove(0, brk + 1);

        if (!line.trimmed().isEmpty())
            m_pending = line;
    }

    if (!m_pending.isEmpty())
        m_debounce->start();
}

// one-shot query, also the path used before the subscriber attaches
void Komorebi::refresh()
{
    const QByteArray out = run({QStringLiteral("state")}, 5000);
    if (out.isEmpty())
        return;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(out, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    applyState(doc.object());
}

void Komorebi::rebuild()
{
    const QByteArray line = m_pending;
    m_pending.clear();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QJsonObject root = doc.object();
    if (!root.contains(QLatin1String("state")))
        return;

    applyState(root.value(QLatin1String("state")).toObject());
}

void Komorebi::applyState(const QJsonObject &state)
{
    const QJsonObject monitorsNode = state.value(QLatin1String("monitors")).toObject();
    const QJsonArray monitorArray = elementsOf(monitorsNode);
    const int focusedMonitorIndex = focusedOf(monitorsNode);

    clearValueObjects();

    m_monitors.clear();
    m_workspaces.clear();
    m_toplevels.clear();
    m_focusedMonitor = nullptr;
    m_focusedWorkspace = nullptr;
    m_activeToplevel = nullptr;

    for (int mi = 0; mi < monitorArray.size(); ++mi) {
        const QJsonObject monitorObject = monitorArray.at(mi).toObject();

        auto *monitor = new KomorebiMonitor(this);
        monitor->setName(monitorObject.value(QLatin1String("name")).toString());
        monitor->setDevice(monitorObject.value(QLatin1String("device")).toString());
        monitor->setId(static_cast<quint64>(monitorObject.value(QLatin1String("id")).toDouble()));
        monitor->setSize(monitorObject.value(QLatin1String("size")).toObject());
        monitor->setWorkArea(monitorObject.value(QLatin1String("work_area_size")).toObject());
        monitor->setActive(mi == focusedMonitorIndex);

        const QJsonObject workspacesNode = monitorObject.value(QLatin1String("workspaces")).toObject();
        const QJsonArray workspaceArray = elementsOf(workspacesNode);
        const int activeWorkspaceIndex = focusedOf(workspacesNode);
        monitor->setActiveWorkspace(activeWorkspaceIndex);

        QVariantList monitorWorkspaces;
        QVariantList monitorToplevels;

        for (int wi = 0; wi < workspaceArray.size(); ++wi) {
            const QJsonObject workspaceObject = workspaceArray.at(wi).toObject();

            auto *workspace = new KomorebiWorkspace(this);
            workspace->setName(workspaceObject.value(QLatin1String("name")).toString());
            workspace->setIndex(wi);
            workspace->setMonitorName(monitor->name());
            workspace->setActive(mi == focusedMonitorIndex && wi == activeWorkspaceIndex);

            const QJsonObject containersNode = workspaceObject.value(QLatin1String("containers")).toObject();
            const QJsonArray containerArray = elementsOf(containersNode);
            const int focusedContainer = focusedOf(containersNode);

            QVariantList workspaceToplevels;

            for (int ci = 0; ci < containerArray.size(); ++ci) {
                const QJsonObject containerObject = containerArray.at(ci).toObject();
                const QJsonObject windowsNode = containerObject.value(QLatin1String("windows")).toObject();
                const QJsonArray windowArray = elementsOf(windowsNode);
                const int focusedWindow = focusedOf(windowsNode);

                for (int k = 0; k < windowArray.size(); ++k) {
                    const QJsonObject windowObject = windowArray.at(k).toObject();

                    auto *toplevel = new KomorebiToplevel(this);
                    toplevel->setTitle(windowObject.value(QLatin1String("title")).toString());
                    toplevel->setExe(windowObject.value(QLatin1String("exe")).toString());
                    toplevel->setCls(windowObject.value(QLatin1String("class")).toString());
                    toplevel->setHwnd(static_cast<quint64>(windowObject.value(QLatin1String("hwnd")).toDouble()));
                    toplevel->setRect(windowObject.value(QLatin1String("rect")).toObject());
                    toplevel->setWorkspaceName(workspace->name());
                    toplevel->setMonitorName(monitor->name());
                    toplevel->setActive(ci == focusedContainer && k == focusedWindow);

                    const QVariant entry = QVariant::fromValue(toplevel);
                    workspaceToplevels.append(entry);
                    monitorToplevels.append(entry);
                    m_toplevels.append(entry);

                    if (toplevel->active() && mi == focusedMonitorIndex)
                        m_activeToplevel = toplevel;
                }
            }

            workspace->setToplevels(workspaceToplevels);
            monitorWorkspaces.append(QVariant::fromValue(workspace));
            m_workspaces.append(QVariant::fromValue(workspace));

            if (workspace->active())
                m_focusedWorkspace = workspace;
        }

        monitor->setWorkspaces(monitorWorkspaces);
        m_monitors.append(QVariant::fromValue(monitor));

        if (monitor->active())
            m_focusedMonitor = monitor;
    }

    emit monitorsChanged();
    emit workspacesChanged();
    emit toplevelsChanged();
    emit focusedMonitorChanged();
    emit focusedWorkspaceChanged();
    emit activeToplevelChanged();
    emit stateChanged();

    setAvailable(true);
}

// the value types are direct children; the server/process/timer are too
void Komorebi::clearValueObjects()
{
    const QList<QObject *> children = findChildren<QObject *>(QString(), Qt::FindDirectChildrenOnly);
    for (QObject *child : children) {
        if (qobject_cast<KomorebiMonitor *>(child) || qobject_cast<KomorebiWorkspace *>(child)
            || qobject_cast<KomorebiToplevel *>(child) || qobject_cast<KomorebiRect *>(child)) {
            delete child;
        }
    }
}

void Komorebi::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    emit availableChanged();
}

// Windows screen names look like \\.\DISPLAY1, komorebi calls that DISPLAY1
KomorebiMonitor *Komorebi::monitorFor(QObject *screen) const
{
    if (!screen)
        return nullptr;

    const QString screenName = screen->property("name").toString();
    if (screenName.isEmpty())
        return nullptr;

    for (const QVariant &entry : m_monitors) {
        auto *monitor = entry.value<KomorebiMonitor *>();
        if (!monitor)
            continue;

        const QString name = monitor->name();
        if (name.compare(screenName, Qt::CaseInsensitive) == 0
            || screenName.contains(name, Qt::CaseInsensitive)) {
            return monitor;
        }
    }

    return m_focusedMonitor;
}

bool Komorebi::dispatch(const QString &command, const QStringList &args) const
{
    if (command.isEmpty())
        return false;

    return QProcess::startDetached(QString::fromLatin1(kExecutable), QStringList{command} + args);
}

QString Komorebi::query(const QString &command, const QStringList &args) const
{
    if (command.isEmpty())
        return {};

    return QString::fromLocal8Bit(run(QStringList{command} + args, 5000)).trimmed();
}

QByteArray Komorebi::run(const QStringList &args, int timeoutMs) const
{
    QProcess proc;
    proc.start(QString::fromLatin1(kExecutable), args);
    if (!proc.waitForStarted(2000) || !proc.waitForFinished(timeoutMs))
        return {};

    return proc.readAllStandardOutput();
}

} // namespace wqs