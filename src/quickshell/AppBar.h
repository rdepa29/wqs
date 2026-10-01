#pragma once

#include <QRect>

class QWindow;

namespace wqs {

/// Reserves workspace for an edge-docked shell window the way the taskbar does, by
/// registering the window as a Windows app-bar (SHAppBarMessage). This is what makes a
/// tiling window manager (komorebi, GlazeWM, ...) and Explorer carve the bar's strip out
/// of the work area, independent of the WM - the same mechanism yasb uses when it sets
/// `windows_app_bar: true`.
///
/// The Windows types live behind an Impl so <windows.h> (and its min/max macros) does not
/// leak into the shell headers. On non-Windows platforms every method is a no-op, so
/// callers do not need their own platform checks.
class AppBar
{
public:
    /// Screen edge the reserved strip is docked to. Mirrors the Windows ABE_* constants
    /// without exposing them.
    enum Edge {
        NoEdge = 0,
        Top = 1,
        Bottom = 2,
        Left = 3,
        Right = 4,
    };

    AppBar();
    ~AppBar();

    AppBar(const AppBar &) = delete;
    AppBar &operator=(const AppBar &) = delete;

    /// Reserve a strip along an edge of the monitor the window currently sits on. The
    /// window's own geometry is left untouched; only the work area reported to the OS
    /// changes. `logicalThickness` <= 0 reserves up to the window's own outer edge
    /// (so any gap between the screen edge and the bar is included); a positive value
    /// reserves that many logical pixels from the edge instead. An empty edge, a null
    /// window, or a hidden window releases any existing reservation.
    void reserveForWindow(QWindow *window, Edge edge, int logicalThickness = 0);

    /// Whether a reservation is currently held.
    bool isReserved() const;

    /// Release the reservation and forget the window.
    void clear();

private:
    void reserve(Edge edge, const QRect &nativeStrip);

    struct Impl;
    Impl *m_impl = nullptr;
};

} // namespace wqs
