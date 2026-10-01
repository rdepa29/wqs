#pragma once

#include <QRect>

class QWindow;

namespace wqs {

// registers a window as a Windows app-bar (SHAppBarMessage) so WM/Explorer
// reserve its strip, like yasb's windows_app_bar. windows.h stays in the pimpl.
class AppBar
{
public:
    // ABE_* edges
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

    // reserve an edge strip on the window's monitor; thickness <= 0 means up to
    // the window's own edge. no edge / null / hidden window releases it.
    void reserveForWindow(QWindow *window, Edge edge, int logicalThickness = 0);

    bool isReserved() const;

    void clear();

private:
    void reserve(Edge edge, const QRect &nativeStrip);

    struct Impl;
    Impl *m_impl = nullptr;
};

} // namespace wqs
