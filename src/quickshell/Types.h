#pragma once

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QtTypes>

namespace wqs {

/// Grouped anchor flags for @@PanelWindow.anchors. A value type so QML can write
/// `anchors { top: true; left: true }` exactly as upstream Quickshell allows.
struct Anchors
{
    Q_GADGET
    QML_VALUE_TYPE(panelAnchors)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(bool left MEMBER left)
    Q_PROPERTY(bool right MEMBER right)
    Q_PROPERTY(bool top MEMBER top)
    Q_PROPERTY(bool bottom MEMBER bottom)

public:
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;

    bool horizontal() const { return left && right; }
    bool vertical() const { return top && bottom; }

    bool operator==(const Anchors &other) const = default;
};

/// Grouped pixel offsets for @@PanelWindow.margins.
struct Margins
{
    Q_GADGET
    QML_VALUE_TYPE(panelMargins)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(qint32 top MEMBER top)
    Q_PROPERTY(qint32 right MEMBER right)
    Q_PROPERTY(qint32 bottom MEMBER bottom)
    Q_PROPERTY(qint32 left MEMBER left)

public:
    qint32 top = 0;
    qint32 right = 0;
    qint32 bottom = 0;
    qint32 left = 0;

    bool operator==(const Margins &other) const = default;
};

/// @@PanelWindow.exclusionMode.
namespace ExclusionMode {
Q_NAMESPACE
QML_ELEMENT

enum Enum : quint8 {
    /// Respect other shell layers' exclusion zones and optionally reserve one.
    Normal = 0,
    /// Ignore other layers and never reserve a zone.
    Ignore = 1,
    /// Reserve space based on the window's anchors and size.
    Auto = 2,
};
Q_ENUM_NS(Enum)

} // namespace ExclusionMode

/// Screen edges, used by @@FloatingWindow.startSystemResize.
namespace Edges {
Q_NAMESPACE
QML_ELEMENT

enum Enum : quint8 {
    None = 0x0,
    Top = 0x1,
    Left = 0x2,
    Right = 0x4,
    Bottom = 0x8,
};
Q_FLAG_NS(Enum)

} // namespace Edges

} // namespace wqs
