#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QtGui/qwindowdefs.h>
#include <QtQmlIntegration/qqmlintegration.h>

#include "AppBar.h"
#include "Screen.h"
#include "Types.h"

#include <QQuickItem>

class QQuickWindow;

namespace wqs {

struct PopupAnchorRect
{
    Q_GADGET
    QML_VALUE_TYPE(popupAnchorRect)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(qint32 x MEMBER x)
    Q_PROPERTY(qint32 y MEMBER y)
    Q_PROPERTY(qint32 width MEMBER width)
    Q_PROPERTY(qint32 height MEMBER height)

public:
    qint32 x = 0;
    qint32 y = 0;
    qint32 width = 0;
    qint32 height = 0;

    bool operator==(const PopupAnchorRect &other) const
    {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

class PopupAnchor : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QObject *window READ window WRITE setWindow NOTIFY anchorChanged)
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY anchorChanged)
    Q_PROPERTY(PopupAnchorRect rect READ rect WRITE setRect NOTIFY anchorChanged)

public:
    explicit PopupAnchor(QObject *parent = nullptr);

    QObject *window() const { return m_window; }
    void setWindow(QObject *window);
    QQuickItem *item() const { return m_item; }
    void setItem(QQuickItem *item);
    PopupAnchorRect rect() const { return m_rect; }
    void setRect(const PopupAnchorRect &rect);

signals:
    void anchorChanged();

private:
    QObject *m_window = nullptr;
    QQuickItem *m_item = nullptr;
    PopupAnchorRect m_rect;
};

class QsWindow : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_NAMED_ELEMENT(QsWindow)
    QML_UNCREATABLE("QsWindow is an abstract base class")
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QQuickItem *contentItem READ contentItem CONSTANT)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool backingWindowVisible READ isBackingWindowVisible NOTIFY backingWindowVisibleChanged)
    Q_PROPERTY(qint32 implicitWidth READ implicitWidth WRITE setImplicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qint32 implicitHeight READ implicitHeight WRITE setImplicitHeight NOTIFY implicitHeightChanged)
    Q_PROPERTY(qint32 width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(qint32 height READ height WRITE setHeight NOTIFY heightChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(wqs::QuickshellScreenInfo *screen READ screen WRITE setScreen NOTIFY screenChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(bool updatesEnabled READ updatesEnabled WRITE setUpdatesEnabled NOTIFY updatesEnabledChanged)
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QsWindow(QObject *parent = nullptr);
    ~QsWindow() override;

    void classBegin() override;
    void componentComplete() override;

    QQuickItem *contentItem() const;
    QQuickWindow *quickWindow() const { return m_window; }

    bool isVisible() const { return m_visible; }
    bool isBackingWindowVisible() const;
    void setVisible(bool visible);

    qint32 implicitWidth() const { return m_implicitWidth; }
    void setImplicitWidth(qint32 width);
    qint32 implicitHeight() const { return m_implicitHeight; }
    void setImplicitHeight(qint32 height);

    qint32 width() const { return m_width; }
    void setWidth(qint32 width);
    qint32 height() const { return m_height; }
    void setHeight(qint32 height);

    qreal devicePixelRatio() const;
    QuickshellScreenInfo *screen() const { return m_screen; }
    virtual void setScreen(QuickshellScreenInfo *screen);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

    bool updatesEnabled() const { return m_updatesEnabled; }
    void setUpdatesEnabled(bool enabled);

    QQmlListProperty<QObject> data();

    Q_INVOKABLE QPointF itemPosition(QQuickItem *item) const;
    Q_INVOKABLE QRectF itemRect(QQuickItem *item) const;
    Q_INVOKABLE QPointF mapFromItem(QQuickItem *item, QPointF point) const;
    Q_INVOKABLE QPointF mapFromItem(QQuickItem *item, qreal x, qreal y) const;
    Q_INVOKABLE QRectF mapFromItem(QQuickItem *item, QRectF rect) const;
    Q_INVOKABLE QRectF mapFromItem(QQuickItem *item, qreal x, qreal y, qreal width, qreal height) const;

signals:
    void closed();
    void windowConnected();
    void visibleChanged();
    void backingWindowVisibleChanged();
    void implicitWidthChanged();
    void implicitHeightChanged();
    void widthChanged();
    void heightChanged();
    void devicePixelRatioChanged();
    void screenChanged();
    void colorChanged();
    void updatesEnabledChanged();

protected:
    virtual Qt::WindowFlags windowFlags() const;
    virtual QString windowTitle() const;
    virtual bool defaultVisible() const { return true; }
    virtual void updateGeometry();
    void relayout();

    void setActualSize(qint32 width, qint32 height);

    QQuickWindow *m_window = nullptr;
    QuickshellScreenInfo *m_screen = nullptr;
    QColor m_color = Qt::white;
    qint32 m_implicitWidth = 0;
    qint32 m_implicitHeight = 0;
    qint32 m_width = 0;
    qint32 m_height = 0;
    bool m_visible = true;
    bool m_visibleExplicit = false;
    bool m_updatesEnabled = true;
    bool m_completed = false;

private:
    static void appendChild(QQmlListProperty<QObject> *prop, QObject *object);
    static qsizetype childCount(QQmlListProperty<QObject> *prop);
    static QObject *childAt(QQmlListProperty<QObject> *prop, qsizetype index);
    static void clearChildren(QQmlListProperty<QObject> *prop);

    QList<QObject *> m_children;
};

class PanelWindow : public QsWindow
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(wqs::Anchors anchors READ anchors WRITE setAnchors NOTIFY anchorsChanged)
    Q_PROPERTY(wqs::Margins margins READ margins WRITE setMargins NOTIFY marginsChanged)
    Q_PROPERTY(qint32 exclusiveZone READ exclusiveZone WRITE setExclusiveZone NOTIFY exclusiveZoneChanged)
    Q_PROPERTY(wqs::ExclusionMode::Enum exclusionMode READ exclusionMode WRITE setExclusionMode NOTIFY exclusionModeChanged)
    Q_PROPERTY(bool aboveWindows READ aboveWindows WRITE setAboveWindows NOTIFY aboveWindowsChanged)
    Q_PROPERTY(bool focusable READ focusable WRITE setFocusable NOTIFY focusableChanged)

public:
    explicit PanelWindow(QObject *parent = nullptr);

    Anchors anchors() const { return m_anchors; }
    void setAnchors(const Anchors &anchors);
    Margins margins() const { return m_margins; }
    void setMargins(const Margins &margins);
    qint32 exclusiveZone() const { return m_exclusiveZone; }
    void setExclusiveZone(qint32 zone);
    ExclusionMode::Enum exclusionMode() const { return m_exclusionMode; }
    void setExclusionMode(ExclusionMode::Enum mode);
    bool aboveWindows() const { return m_aboveWindows; }
    void setAboveWindows(bool above);
    bool focusable() const { return m_focusable; }
    void setFocusable(bool focusable);

signals:
    void anchorsChanged();
    void marginsChanged();
    void exclusiveZoneChanged();
    void exclusionModeChanged();
    void aboveWindowsChanged();
    void focusableChanged();

protected:
    Qt::WindowFlags windowFlags() const override;
    QString windowTitle() const override;
    void updateGeometry() override;

private:
    void updateAppBar();

    Anchors m_anchors;
    Margins m_margins;
    qint32 m_exclusiveZone = 0;
    ExclusionMode::Enum m_exclusionMode = ExclusionMode::Auto;
    bool m_aboveWindows = true;
    bool m_focusable = false;
    AppBar m_appBar;
};

class FloatingWindow : public QsWindow
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QSize minimumSize READ minimumSize WRITE setMinimumSize NOTIFY minimumSizeChanged)
    Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize NOTIFY maximumSizeChanged)
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized NOTIFY minimizedChanged)
    Q_PROPERTY(bool maximized READ isMaximized WRITE setMaximized NOTIFY maximizedChanged)
    Q_PROPERTY(bool fullscreen READ isFullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(QObject *parentWindow READ parentWindow WRITE setParentWindow NOTIFY parentWindowChanged)
    Q_PROPERTY(qint32 x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(qint32 y READ y WRITE setY NOTIFY yChanged)

public:
    explicit FloatingWindow(QObject *parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QSize minimumSize() const { return m_minimumSize; }
    void setMinimumSize(const QSize &size);
    QSize maximumSize() const { return m_maximumSize; }
    void setMaximumSize(const QSize &size);
    bool isMinimized() const { return m_minimized; }
    void setMinimized(bool minimized);
    bool isMaximized() const { return m_maximized; }
    void setMaximized(bool maximized);
    bool isFullscreen() const { return m_fullscreen; }
    void setFullscreen(bool fullscreen);
    QObject *parentWindow() const { return m_parentWindow; }
    void setParentWindow(QObject *window);
    qint32 x() const { return m_x; }
    void setX(qint32 x);
    qint32 y() const { return m_y; }
    void setY(qint32 y);

    Q_INVOKABLE bool startSystemMove();
    Q_INVOKABLE bool startSystemResize(int edges);

signals:
    void titleChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();
    void minimizedChanged();
    void maximizedChanged();
    void fullscreenChanged();
    void parentWindowChanged();
    void xChanged();
    void yChanged();

protected:
    Qt::WindowFlags windowFlags() const override;
    QString windowTitle() const override;
    void updateGeometry() override;

private:
    QString m_title = QStringLiteral("wqs-window");
    QSize m_minimumSize;
    QSize m_maximumSize;
    bool m_minimized = false;
    bool m_maximized = false;
    bool m_fullscreen = false;
    QObject *m_parentWindow = nullptr;
    qint32 m_x = -1;
    qint32 m_y = -1;
    bool m_positioned = false;
};

/// Port of `PopupWindow`, a window positioned relative to another window or item.
class PopupWindow : public QsWindow
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject *parentWindow READ parentWindow WRITE setParentWindow NOTIFY parentWindowChanged)
    Q_PROPERTY(qint32 relativeX READ relativeX WRITE setRelativeX NOTIFY relativeXChanged)
    Q_PROPERTY(qint32 relativeY READ relativeY WRITE setRelativeY NOTIFY relativeYChanged)
    Q_PROPERTY(wqs::PopupAnchor *anchor READ anchor CONSTANT)
    Q_PROPERTY(bool grabFocus READ grabFocus WRITE setGrabFocus NOTIFY grabFocusChanged)

public:
    explicit PopupWindow(QObject *parent = nullptr);

    QObject *parentWindow() const { return m_parentWindow; }
    void setParentWindow(QObject *window);
    qint32 relativeX() const { return m_relativeX; }
    void setRelativeX(qint32 x);
    qint32 relativeY() const { return m_relativeY; }
    void setRelativeY(qint32 y);
    PopupAnchor *anchor() const { return m_anchor; }
    bool grabFocus() const { return m_grabFocus; }
    void setGrabFocus(bool grab);

signals:
    void parentWindowChanged();
    void relativeXChanged();
    void relativeYChanged();
    void grabFocusChanged();

protected:
    Qt::WindowFlags windowFlags() const override;
    QString windowTitle() const override;
    bool defaultVisible() const override { return false; }
    void updateGeometry() override;

private:
    void onAnchorChanged();

    QObject *m_parentWindow = nullptr;
    qint32 m_relativeX = 0;
    qint32 m_relativeY = 0;
    PopupAnchor *m_anchor = nullptr;
    bool m_grabFocus = true;
};

} // namespace wqs
