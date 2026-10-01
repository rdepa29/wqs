#include "Window.h"

#include <QGuiApplication>
#include <QPointF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QScreen>
#include <QtGui/qguiapplication.h>

#include "Screen.h"

namespace wqs {

PopupAnchor::PopupAnchor(QObject *parent)
    : QObject(parent)
{
}

void PopupAnchor::setWindow(QObject *window)
{
    if (m_window == window)
        return;
    m_window = window;
    emit anchorChanged();
}

void PopupAnchor::setItem(QQuickItem *item)
{
    if (m_item == item)
        return;
    m_item = item;
    emit anchorChanged();
}

void PopupAnchor::setRect(const PopupAnchorRect &rect)
{
    if (m_rect == rect)
        return;
    m_rect = rect;
    emit anchorChanged();
}

QsWindow::QsWindow(QObject *parent)
    : QObject(parent)
{
    m_window = new QQuickWindow();
    static_cast<QObject *>(m_window)->setParent(this);

    connect(m_window, &QWindow::visibleChanged, this, &QsWindow::backingWindowVisibleChanged);
    connect(m_window, &QWindow::screenChanged, this, &QsWindow::devicePixelRatioChanged);
    connect(m_window, &QQuickWindow::closing, this, [this](QQuickCloseEvent *) {
        m_window->setVisible(false);
        emit closed();
    });
}

QsWindow::~QsWindow() = default;

void QsWindow::classBegin()
{
}

void QsWindow::componentComplete()
{
    m_completed = true;

    m_window->setFlags(windowFlags());
    m_window->setTitle(windowTitle());
    m_window->setColor(m_color);

    if (!m_visibleExplicit)
        m_visible = defaultVisible();

    updateGeometry();
    m_window->setVisible(m_visible);

    emit windowConnected();
}

QQuickItem *QsWindow::contentItem() const
{
    return m_window->contentItem();
}

bool QsWindow::isBackingWindowVisible() const
{
    return m_window->isVisible();
}

void QsWindow::setVisible(bool visible)
{
    m_visibleExplicit = true;
    if (m_visible == visible)
        return;
    m_visible = visible;
    if (m_completed)
        m_window->setVisible(visible);
    emit visibleChanged();
}

void QsWindow::setImplicitWidth(qint32 width)
{
    if (m_implicitWidth == width)
        return;
    m_implicitWidth = width;
    emit implicitWidthChanged();
    relayout();
}

void QsWindow::setImplicitHeight(qint32 height)
{
    if (m_implicitHeight == height)
        return;
    m_implicitHeight = height;
    emit implicitHeightChanged();
    relayout();
}

void QsWindow::setWidth(qint32 width)
{
    setImplicitWidth(width);
}

void QsWindow::setHeight(qint32 height)
{
    setImplicitHeight(height);
}

qreal QsWindow::devicePixelRatio() const
{
    return m_window->devicePixelRatio();
}

void QsWindow::setScreen(QuickshellScreenInfo *screen)
{
    if (m_screen == screen)
        return;
    m_screen = screen;
    if (screen && screen->screen())
        m_window->setScreen(screen->screen());
    emit screenChanged();
    relayout();
}

void QsWindow::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    m_window->setColor(color);
    emit colorChanged();
}

void QsWindow::setUpdatesEnabled(bool enabled)
{
    if (m_updatesEnabled == enabled)
        return;
    m_updatesEnabled = enabled;
    m_window->setVisible(m_visible && enabled);
    emit updatesEnabledChanged();
}

QQmlListProperty<QObject> QsWindow::data()
{
    return QQmlListProperty<QObject>(this, nullptr, &QsWindow::appendChild, &QsWindow::childCount,
                                     &QsWindow::childAt, &QsWindow::clearChildren);
}

QPointF QsWindow::itemPosition(QQuickItem *item) const
{
    return mapFromItem(item, QPointF(0, 0));
}

QRectF QsWindow::itemRect(QQuickItem *item) const
{
    return QRectF(itemPosition(item), QSizeF(item->width(), item->height()));
}

QPointF QsWindow::mapFromItem(QQuickItem *item, QPointF point) const
{
    if (!item)
        return point;
    return item->mapToItem(contentItem(), point);
}

QPointF QsWindow::mapFromItem(QQuickItem *item, qreal x, qreal y) const
{
    return mapFromItem(item, QPointF(x, y));
}

QRectF QsWindow::mapFromItem(QQuickItem *item, QRectF rect) const
{
    return QRectF(mapFromItem(item, rect.topLeft()), rect.size());
}

QRectF QsWindow::mapFromItem(QQuickItem *item, qreal x, qreal y, qreal width, qreal height) const
{
    return mapFromItem(item, QRectF(x, y, width, height));
}

Qt::WindowFlags QsWindow::windowFlags() const
{
    return Qt::FramelessWindowHint | Qt::Tool;
}

QString QsWindow::windowTitle() const
{
    return QStringLiteral("wqs-window");
}

void QsWindow::updateGeometry()
{
    setActualSize(qMax(0, m_implicitWidth), qMax(0, m_implicitHeight));
}

void QsWindow::relayout()
{
    if (m_completed)
        updateGeometry();
}

void QsWindow::setActualSize(qint32 width, qint32 height)
{
    width = qMax(0, width);
    height = qMax(0, height);
    if (m_width != width) {
        m_width = width;
        m_window->setWidth(width);
        emit widthChanged();
    }
    if (m_height != height) {
        m_height = height;
        m_window->setHeight(height);
        emit heightChanged();
    }
}

void QsWindow::appendChild(QQmlListProperty<QObject> *prop, QObject *object)
{
    auto *self = qobject_cast<QsWindow *>(prop->object);
    if (!self || !object)
        return;
    if (auto *item = qobject_cast<QQuickItem *>(object)) {
        item->setParentItem(self->contentItem());
    } else {
        object->setParent(self);
    }
    self->m_children.append(object);
}

qsizetype QsWindow::childCount(QQmlListProperty<QObject> *prop)
{
    auto *self = static_cast<QsWindow *>(prop->object);
    return self ? self->m_children.size() : 0;
}

QObject *QsWindow::childAt(QQmlListProperty<QObject> *prop, qsizetype index)
{
    auto *self = static_cast<QsWindow *>(prop->object);
    if (!self || index < 0 || index >= self->m_children.size())
        return nullptr;
    return self->m_children.at(index);
}

void QsWindow::clearChildren(QQmlListProperty<QObject> *prop)
{
    auto *self = static_cast<QsWindow *>(prop->object);
    if (!self)
        return;
    for (QObject *child : self->m_children) {
        if (auto *item = qobject_cast<QQuickItem *>(child))
            item->setParentItem(nullptr);
        child->deleteLater();
    }
    self->m_children.clear();
}

// === PanelWindow ===========================================================

PanelWindow::PanelWindow(QObject *parent)
    : QsWindow(parent)
{
    // The bar is (re)registered as a Windows app-bar once the native window is shown; the
    // base class hides the window until componentComplete, so react to visibility here.
    connect(m_window, &QWindow::visibleChanged, this, [this] { updateAppBar(); });
}

void PanelWindow::setAnchors(const Anchors &anchors)
{
    if (m_anchors == anchors)
        return;
    m_anchors = anchors;
    emit anchorsChanged();
    relayout();
}

void PanelWindow::setMargins(const Margins &margins)
{
    if (m_margins == margins)
        return;
    m_margins = margins;
    emit marginsChanged();
    relayout();
}

void PanelWindow::setExclusiveZone(qint32 zone)
{
    if (m_exclusiveZone == zone)
        return;
    m_exclusiveZone = zone;
    emit exclusiveZoneChanged();
    updateAppBar();
}

void PanelWindow::setExclusionMode(ExclusionMode::Enum mode)
{
    if (m_exclusionMode == mode)
        return;
    m_exclusionMode = mode;
    emit exclusionModeChanged();
    updateAppBar();
}

void PanelWindow::setAboveWindows(bool above)
{
    if (m_aboveWindows == above)
        return;
    m_aboveWindows = above;
    emit aboveWindowsChanged();
    if (m_completed)
        m_window->setFlags(windowFlags());
    updateAppBar();
}

void PanelWindow::setFocusable(bool focusable)
{
    if (m_focusable == focusable)
        return;
    m_focusable = focusable;
    emit focusableChanged();
    if (m_completed)
        m_window->setFlags(windowFlags());
    updateAppBar();
}

Qt::WindowFlags PanelWindow::windowFlags() const
{
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
    if (m_aboveWindows)
        flags |= Qt::WindowStaysOnTopHint;
    if (!m_focusable)
        flags |= Qt::WindowDoesNotAcceptFocus;
    return flags;
}

QString PanelWindow::windowTitle() const
{
    // Every wqs window that is not explicitly tagged "-dwm" is an obstacle: komorebi (and
    // compatibles) ignore it, and edge-anchored panels additionally register as a Windows
    // app-bar (see updateAppBar) so the work area is reserved like the taskbar's.
    return QStringLiteral("wqs-window");
}

void PanelWindow::updateGeometry()
{
    QScreen *native = (m_screen && m_screen->screen()) ? m_screen->screen()
                                                       : QGuiApplication::primaryScreen();
    const QRect avail = native ? native->geometry() : QRect(0, 0, 0, 0);

    const bool horizontal = m_anchors.left && m_anchors.right;
    const bool vertical = m_anchors.top && m_anchors.bottom;

    const qint32 w = horizontal ? avail.width() - m_margins.left - m_margins.right
                                : m_implicitWidth;
    const qint32 h = vertical ? avail.height() - m_margins.top - m_margins.bottom
                              : m_implicitHeight;

    qint32 x;
    if (m_anchors.left)
        x = avail.x() + m_margins.left;
    else if (m_anchors.right)
        x = avail.x() + avail.width() - m_margins.right - w;
    else
        x = avail.x() + (avail.width() - w) / 2;

    qint32 y;
    if (m_anchors.top)
        y = avail.y() + m_margins.top;
    else if (m_anchors.bottom)
        y = avail.y() + avail.height() - m_margins.bottom - h;
    else
        y = avail.y() + (avail.height() - h) / 2;

    setActualSize(w, h);
    m_window->setPosition(x, y);
    updateAppBar();
}

void PanelWindow::updateAppBar()
{
    if (!m_completed || !m_window || !m_window->isVisible()) {
        m_appBar.clear();
        return;
    }

    // Ignore mode never reserves space, matching Quickshell's exclusionMode semantics.
    if (m_exclusionMode == ExclusionMode::Ignore) {
        m_appBar.clear();
        return;
    }

    AppBar::Edge edge = AppBar::NoEdge;
    if (m_anchors.top)
        edge = AppBar::Top;
    else if (m_anchors.bottom)
        edge = AppBar::Bottom;
    else if (m_anchors.left)
        edge = AppBar::Left;
    else if (m_anchors.right)
        edge = AppBar::Right;

    // Only edges can host an app-bar; a free-floating panel reserves nothing.
    m_appBar.reserveForWindow(m_window, edge, m_exclusiveZone);
}

// === FloatingWindow ========================================================

FloatingWindow::FloatingWindow(QObject *parent)
    : QsWindow(parent)
{
}

void FloatingWindow::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    m_window->setTitle(title);
    emit titleChanged();
}

void FloatingWindow::setMinimumSize(const QSize &size)
{
    if (m_minimumSize == size)
        return;
    m_minimumSize = size;
    m_window->setMinimumSize(size);
    emit minimumSizeChanged();
}

void FloatingWindow::setMaximumSize(const QSize &size)
{
    if (m_maximumSize == size)
        return;
    m_maximumSize = size;
    m_window->setMaximumSize(size);
    emit maximumSizeChanged();
}

void FloatingWindow::setMinimized(bool minimized)
{
    if (m_minimized == minimized)
        return;
    m_minimized = minimized;
    if (m_completed)
        m_window->setVisibility(minimized ? QWindow::Minimized : QWindow::Windowed);
    emit minimizedChanged();
}

void FloatingWindow::setMaximized(bool maximized)
{
    if (m_maximized == maximized)
        return;
    m_maximized = maximized;
    if (m_completed)
        m_window->setVisibility(maximized ? QWindow::Maximized : QWindow::Windowed);
    emit maximizedChanged();
}

void FloatingWindow::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen)
        return;
    m_fullscreen = fullscreen;
    if (m_completed)
        m_window->setVisibility(fullscreen ? QWindow::FullScreen : QWindow::Windowed);
    emit fullscreenChanged();
}

void FloatingWindow::setParentWindow(QObject *window)
{
    if (m_parentWindow == window)
        return;
    m_parentWindow = window;
    emit parentWindowChanged();
}

void FloatingWindow::setX(qint32 x)
{
    if (m_x == x)
        return;
    m_x = x;
    m_positioned = true;
    emit xChanged();
    relayout();
}

void FloatingWindow::setY(qint32 y)
{
    if (m_y == y)
        return;
    m_y = y;
    m_positioned = true;
    emit yChanged();
    relayout();
}

bool FloatingWindow::startSystemMove()
{
    return m_window->startSystemMove();
}

bool FloatingWindow::startSystemResize(int edges)
{
    return m_window->startSystemResize(Qt::Edges(edges));
}

Qt::WindowFlags FloatingWindow::windowFlags() const
{
    return Qt::Window;
}

QString FloatingWindow::windowTitle() const
{
    return m_title;
}

void FloatingWindow::updateGeometry()
{
    QScreen *native = (m_screen && m_screen->screen()) ? m_screen->screen()
                                                       : QGuiApplication::primaryScreen();
    const QRect avail = native ? native->geometry() : QRect(0, 0, 0, 0);

    const qint32 w = qMax(0, m_implicitWidth);
    const qint32 h = qMax(0, m_implicitHeight);

    const bool centered = !m_positioned;
    const qint32 x = centered ? avail.x() + (avail.width() - w) / 2 : m_x;
    const qint32 y = centered ? avail.y() + (avail.height() - h) / 2 : m_y;

    setActualSize(w, h);
    m_window->setPosition(x, y);
}

// === PopupWindow ===========================================================

PopupWindow::PopupWindow(QObject *parent)
    : QsWindow(parent)
    , m_anchor(new PopupAnchor(this))
{
    connect(m_anchor, &PopupAnchor::anchorChanged, this, &PopupWindow::onAnchorChanged);
}

void PopupWindow::setParentWindow(QObject *window)
{
    if (m_parentWindow == window)
        return;
    m_parentWindow = window;
    m_anchor->setWindow(window);
    emit parentWindowChanged();
    relayout();
}

void PopupWindow::setRelativeX(qint32 x)
{
    if (m_relativeX == x)
        return;
    m_relativeX = x;
    emit relativeXChanged();
    relayout();
}

void PopupWindow::setRelativeY(qint32 y)
{
    if (m_relativeY == y)
        return;
    m_relativeY = y;
    emit relativeYChanged();
    relayout();
}

void PopupWindow::setGrabFocus(bool grab)
{
    if (m_grabFocus == grab)
        return;
    m_grabFocus = grab;
    emit grabFocusChanged();
}

void PopupWindow::onAnchorChanged()
{
    if (m_parentWindow != m_anchor->window()) {
        m_parentWindow = m_anchor->window();
        emit parentWindowChanged();
    }
    relayout();
}

Qt::WindowFlags PopupWindow::windowFlags() const
{
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint;
    if (!m_grabFocus)
        flags |= Qt::WindowDoesNotAcceptFocus;
    return flags;
}

QString PopupWindow::windowTitle() const
{
    return QStringLiteral("wqs-window-dwm");
}

void PopupWindow::updateGeometry()
{
    const qint32 w = qMax(0, m_implicitWidth);
    const qint32 h = qMax(0, m_implicitHeight);

    qint32 x = 0;
    qint32 y = 0;

    if (auto *parent = qobject_cast<QsWindow *>(m_parentWindow)) {
        if (QQuickWindow *pwin = parent->quickWindow()) {
            const QPoint topLeft = pwin->position();
            const PopupAnchorRect rect = m_anchor->rect();
            x = topLeft.x() + m_relativeX + rect.x;
            y = topLeft.y() + m_relativeY + rect.y;
        }
    }

    setActualSize(w, h);
    m_window->setPosition(x, y);
}

} // namespace wqs
