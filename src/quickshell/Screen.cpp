#include "Screen.h"

#include <QRect>
#include <QScreen>

namespace wqs {

QuickshellScreenInfo::QuickshellScreenInfo(QScreen *screen, QObject *parent)
    : QObject(parent)
    , m_screen(screen)
{
    connect(screen, &QScreen::geometryChanged, this, &QuickshellScreenInfo::geometryChanged);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this,
            &QuickshellScreenInfo::densityChanged);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this,
            &QuickshellScreenInfo::densityChanged);
    connect(screen, &QScreen::orientationChanged, this,
            &QuickshellScreenInfo::orientationChanged);
    connect(screen, &QScreen::primaryOrientationChanged, this,
            &QuickshellScreenInfo::primaryOrientationChanged);
}

QString QuickshellScreenInfo::name() const
{
    return m_screen->name();
}

QString QuickshellScreenInfo::model() const
{
    return m_screen->model();
}

QString QuickshellScreenInfo::serialNumber() const
{
    return m_screen->serialNumber();
}

qint32 QuickshellScreenInfo::x() const
{
    return m_screen->geometry().x();
}

qint32 QuickshellScreenInfo::y() const
{
    return m_screen->geometry().y();
}

qint32 QuickshellScreenInfo::width() const
{
    return m_screen->geometry().width();
}

qint32 QuickshellScreenInfo::height() const
{
    return m_screen->geometry().height();
}

qreal QuickshellScreenInfo::physicalPixelDensity() const
{
    return m_screen->physicalDotsPerInch() / 25.4;
}


qreal QuickshellScreenInfo::logicalPixelDensity() const
{
    return m_screen->logicalDotsPerInch() / 25.4;
}

qreal QuickshellScreenInfo::devicePixelRatio() const
{
    return m_screen->devicePixelRatio();
}

Qt::ScreenOrientation QuickshellScreenInfo::orientation() const
{
    return m_screen->orientation();
}

Qt::ScreenOrientation QuickshellScreenInfo::primaryOrientation() const
{
    return m_screen->primaryOrientation();
}

QString QuickshellScreenInfo::toString() const
{
    return QStringLiteral("ShellScreen(%1, %2x%3+%4+%5)")
        .arg(name())
        .arg(width())
        .arg(height())
        .arg(x())
        .arg(y());
}

} // namespace wqs
