#pragma once

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

class QScreen;

namespace wqs {

class QuickshellScreenInfo : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ShellScreen)
    QML_UNCREATABLE("ShellScreen can only be obtained via Quickshell.screens")

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString model READ model CONSTANT)
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)
    Q_PROPERTY(qint32 x READ x NOTIFY geometryChanged)
    Q_PROPERTY(qint32 y READ y NOTIFY geometryChanged)
    Q_PROPERTY(qint32 width READ width NOTIFY geometryChanged)
    Q_PROPERTY(qint32 height READ height NOTIFY geometryChanged)
    Q_PROPERTY(qreal physicalPixelDensity READ physicalPixelDensity NOTIFY densityChanged)
    Q_PROPERTY(qreal logicalPixelDensity READ logicalPixelDensity NOTIFY densityChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio NOTIFY densityChanged)
    Q_PROPERTY(Qt::ScreenOrientation orientation READ orientation NOTIFY orientationChanged)
    Q_PROPERTY(Qt::ScreenOrientation primaryOrientation READ primaryOrientation NOTIFY primaryOrientationChanged)

public:
    explicit QuickshellScreenInfo(QScreen *screen, QObject *parent = nullptr);

    QScreen *screen() const { return m_screen; }

    QString name() const;
    QString model() const;
    QString serialNumber() const;
    qint32 x() const;
    qint32 y() const;
    qint32 width() const;
    qint32 height() const;
    qreal physicalPixelDensity() const;
    qreal logicalPixelDensity() const;
    qreal devicePixelRatio() const;
    Qt::ScreenOrientation orientation() const;
    Qt::ScreenOrientation primaryOrientation() const;

    Q_INVOKABLE QString toString() const;

signals:
    void geometryChanged();
    void densityChanged();
    void orientationChanged();
    void primaryOrientationChanged();

private:
    QScreen *m_screen = nullptr;
};

} // namespace wqs
