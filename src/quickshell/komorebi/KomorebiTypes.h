#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QRect>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace wqs {

class KomorebiRect : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KomorebiRect)
    QML_UNCREATABLE("rectangles come from the window manager")

    Q_PROPERTY(int x READ x CONSTANT)
    Q_PROPERTY(int y READ y CONSTANT)
    Q_PROPERTY(int width READ width CONSTANT)
    Q_PROPERTY(int height READ height CONSTANT)

public:
    explicit KomorebiRect(QObject *parent = nullptr)
        : QObject(parent) { }

    int x() const { return m_rect.x(); }
    int y() const { return m_rect.y(); }
    int width() const { return m_rect.width(); }
    int height() const { return m_rect.height(); }
    QRect rect() const { return m_rect; }

    void setRect(const QJsonObject &o)
    {
        const int l = o.value(QLatin1String("left")).toInt();
        const int t = o.value(QLatin1String("top")).toInt();
        const int r = o.value(QLatin1String("right")).toInt();
        const int b = o.value(QLatin1String("bottom")).toInt();
        m_rect = QRect(l, t, r - l, b - t);
    }

private:
    QRect m_rect;
};

class KomorebiToplevel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KomorebiToplevel)
    QML_UNCREATABLE("toplevels come from the window manager")

    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString exe READ exe CONSTANT)
    Q_PROPERTY(QString cls READ cls CONSTANT)
    Q_PROPERTY(quint64 hwnd READ hwnd CONSTANT)
    Q_PROPERTY(KomorebiRect *rect READ rect CONSTANT)
    Q_PROPERTY(QString workspaceName READ workspaceName CONSTANT)
    Q_PROPERTY(QString monitorName READ monitorName CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool minimized READ minimized CONSTANT)

public:
    explicit KomorebiToplevel(QObject *parent = nullptr)
        : QObject(parent)
        , m_rect(new KomorebiRect(this)) { }

    QString title() const { return m_title; }
    QString exe() const { return m_exe; }
    QString cls() const { return m_cls; }
    quint64 hwnd() const { return m_hwnd; }
    KomorebiRect *rect() const { return m_rect; }
    QString workspaceName() const { return m_workspaceName; }
    QString monitorName() const { return m_monitorName; }
    bool active() const { return m_active; }
    bool minimized() const { return m_minimized; }

    void setActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        emit activeChanged();
    }

    void setTitle(const QString &title) { m_title = title; }
    void setExe(const QString &exe) { m_exe = exe; }
    void setCls(const QString &cls) { m_cls = cls; }
    void setHwnd(quint64 hwnd) { m_hwnd = hwnd; }
    void setRect(const QJsonObject &o) { m_rect->setRect(o); }
    void setWorkspaceName(const QString &name) { m_workspaceName = name; }
    void setMonitorName(const QString &name) { m_monitorName = name; }
    void setMinimized(bool minimized) { m_minimized = minimized; }

signals:
    void activeChanged();

private:
    QString m_title;
    QString m_exe;
    QString m_cls;
    quint64 m_hwnd = 0;
    QString m_workspaceName;
    QString m_monitorName;
    KomorebiRect *m_rect;
    bool m_active = false;
    bool m_minimized = false;
};

class KomorebiWorkspace : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KomorebiWorkspace)
    QML_UNCREATABLE("workspaces come from the window manager")

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(int index READ index CONSTANT)
    Q_PROPERTY(QString monitorName READ monitorName CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int windowCount READ windowCount CONSTANT)
    Q_PROPERTY(QVariantList toplevels READ toplevels NOTIFY toplevelsChanged)

public:
    explicit KomorebiWorkspace(QObject *parent = nullptr)
        : QObject(parent) { }

    QString name() const { return m_name; }
    int index() const { return m_index; }
    QString monitorName() const { return m_monitorName; }
    bool active() const { return m_active; }
    int windowCount() const { return m_toplevels.size(); }
    QVariantList toplevels() const { return m_toplevels; }

    void setActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        emit activeChanged();
    }

    void setName(const QString &name) { m_name = name; }
    void setIndex(int index) { m_index = index; }
    void setMonitorName(const QString &name) { m_monitorName = name; }
    void setToplevels(const QVariantList &tops)
    {
        m_toplevels = tops;
        emit toplevelsChanged();
    }

signals:
    void activeChanged();
    void toplevelsChanged();

private:
    QString m_name;
    int m_index = 0;
    QString m_monitorName;
    QVariantList m_toplevels;
    bool m_active = false;
};

class KomorebiMonitor : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KomorebiMonitor)
    QML_UNCREATABLE("monitors come from the window manager")

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(quint64 id READ id CONSTANT)
    Q_PROPERTY(QString device READ device CONSTANT)
    Q_PROPERTY(KomorebiRect *size READ size CONSTANT)
    Q_PROPERTY(KomorebiRect *workArea READ workArea CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int activeWorkspace READ activeWorkspace CONSTANT)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY workspacesChanged)

public:
    explicit KomorebiMonitor(QObject *parent = nullptr)
        : QObject(parent)
        , m_size(new KomorebiRect(this))
        , m_workArea(new KomorebiRect(this)) { }

    QString name() const { return m_name; }
    quint64 id() const { return m_id; }
    QString device() const { return m_device; }
    KomorebiRect *size() const { return m_size; }
    KomorebiRect *workArea() const { return m_workArea; }
    bool active() const { return m_active; }
    int activeWorkspace() const { return m_activeWorkspace; }
    QVariantList workspaces() const { return m_workspaces; }

    void setActive(bool active)
    {
        if (m_active == active)
            return;
        m_active = active;
        emit activeChanged();
    }

    void setName(const QString &name) { m_name = name; }
    void setId(quint64 id) { m_id = id; }
    void setDevice(const QString &device) { m_device = device; }
    void setSize(const QJsonObject &o) { m_size->setRect(o); }
    void setWorkArea(const QJsonObject &o) { m_workArea->setRect(o); }
    void setActiveWorkspace(int index) { m_activeWorkspace = index; }
    void setWorkspaces(const QVariantList &workspaces)
    {
        m_workspaces = workspaces;
        emit workspacesChanged();
    }

signals:
    void activeChanged();
    void workspacesChanged();

private:
    QString m_name;
    QString m_device;
    quint64 m_id = 0;
    KomorebiRect *m_size;
    KomorebiRect *m_workArea;
    QVariantList m_workspaces;
    int m_activeWorkspace = -1;
    bool m_active = false;
};

inline QJsonArray elementsOf(const QJsonValue &node)
{
    return node.isObject() ? node.toObject().value(QLatin1String("elements")).toArray() : QJsonArray();
}

inline int focusedOf(const QJsonValue &node)
{
    return node.isObject() ? node.toObject().value(QLatin1String("focused")).toInt(-1) : -1;
}

} // namespace wqs