#include "AppBar.h"

#include <QWindow>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace wqs {

#ifdef Q_OS_WIN

struct AppBar::Impl {
    HWND hwnd = nullptr;
    bool active = false; // ABM_NEW has been accepted and not yet balanced by ABM_REMOVE

    static UINT toAbe(AppBar::Edge edge)
    {
        switch (edge) {
        case AppBar::Top: return ABE_TOP;
        case AppBar::Bottom: return ABE_BOTTOM;
        case AppBar::Left: return ABE_LEFT;
        case AppBar::Right: return ABE_RIGHT;
        default: return 0;
        }
    }

    void removeBar()
    {
        if (hwnd && active) {
            APPBARDATA abd{};
            abd.cbSize = sizeof(abd);
            abd.hWnd = hwnd;
            SHAppBarMessage(ABM_REMOVE, &abd);
        }
        active = false;
        hwnd = nullptr;
    }
};

AppBar::AppBar()
    : m_impl(new Impl) { }

AppBar::~AppBar()
{
    m_impl->removeBar();
    delete m_impl;
}

void AppBar::reserveForWindow(QWindow *window, Edge edge, int logicalThickness)
{
    if (!window || edge == NoEdge || !window->isVisible()) {
        clear();
        return;
    }

    auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        clear();
        return;
    }

    // A changed HWND means Qt recreated the native window (e.g. a QWindow flag changed);
    // drop the stale registration before reusing the handle.
    if (m_impl->hwnd != hwnd) {
        m_impl->removeBar();
        m_impl->hwnd = hwnd;
    }

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    RECT wr{};
    if (!monitor || !GetMonitorInfo(monitor, &mi) || !GetWindowRect(hwnd, &wr)) {
        clear();
        return;
    }

    const RECT &m = mi.rcMonitor;
    const int monitorLeft = m.left;
    const int monitorTop = m.top;
    const int monitorRight = m.right;
    const int monitorBottom = m.bottom;

    const qreal dpr = window->devicePixelRatio();
    const int thickness = logicalThickness > 0 ? qRound(logicalThickness * dpr) : 0;

    QRect strip;
    switch (edge) {
    case Top: {
        const int bottom = thickness > 0 ? monitorTop + thickness : wr.bottom;
        strip = QRect(monitorLeft, monitorTop, monitorRight - monitorLeft, bottom - monitorTop);
        break;
    }
    case Bottom: {
        const int top = thickness > 0 ? monitorBottom - thickness : wr.top;
        strip = QRect(monitorLeft, top, monitorRight - monitorLeft, monitorBottom - top);
        break;
    }
    case Left: {
        const int right = thickness > 0 ? monitorLeft + thickness : wr.right;
        strip = QRect(monitorLeft, monitorTop, right - monitorLeft, monitorBottom - monitorTop);
        break;
    }
    case Right: {
        const int left = thickness > 0 ? monitorRight - thickness : wr.left;
        strip = QRect(left, monitorTop, monitorRight - left, monitorBottom - monitorTop);
        break;
    }
    default:
        clear();
        return;
    }

    reserve(edge, strip);
}

void AppBar::reserve(Edge edge, const QRect &nativeStrip)
{
    Impl &im = *m_impl;

    if (!im.hwnd || edge == NoEdge || nativeStrip.isEmpty()) {
        if (im.hwnd && im.active) {
            APPBARDATA abd{};
            abd.cbSize = sizeof(abd);
            abd.hWnd = im.hwnd;
            SHAppBarMessage(ABM_REMOVE, &abd);
            im.active = false;
        }
        return;
    }

    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    abd.hWnd = im.hwnd;

    if (!im.active) {
        if (!SHAppBarMessage(ABM_NEW, &abd))
            return; // registration refused; leave the work area alone
        im.active = true;
    }

    abd.uEdge = Impl::toAbe(edge);
    abd.rc.left = nativeStrip.left();
    abd.rc.top = nativeStrip.top();
    abd.rc.right = nativeStrip.right() + 1;
    abd.rc.bottom = nativeStrip.bottom() + 1;

    SHAppBarMessage(ABM_QUERYPOS, &abd);

    // QUERYPOS slides the rect along the edge to a valid position but can shrink other
    // sides; restore the requested span/thickness so the reservation covers what we asked
    // for, then commit it.
    switch (abd.uEdge) {
    case ABE_TOP: abd.rc.bottom = abd.rc.top + nativeStrip.height(); break;
    case ABE_BOTTOM: abd.rc.top = abd.rc.bottom - nativeStrip.height(); break;
    case ABE_LEFT: abd.rc.right = abd.rc.left + nativeStrip.width(); break;
    case ABE_RIGHT: abd.rc.left = abd.rc.right - nativeStrip.width(); break;
    default: break;
    }

    SHAppBarMessage(ABM_SETPOS, &abd);
}

bool AppBar::isReserved() const { return m_impl->active; }

void AppBar::clear() { m_impl->removeBar(); }

#else // !Q_OS_WIN

struct AppBar::Impl { };

AppBar::AppBar()
    : m_impl(new Impl) { }

AppBar::~AppBar() { delete m_impl; }

void AppBar::reserveForWindow(QWindow *, Edge, int) { }
bool AppBar::isReserved() const { return false; }
void AppBar::reserve(Edge, const QRect &) { }

void AppBar::clear() { }

#endif

} // namespace wqs
