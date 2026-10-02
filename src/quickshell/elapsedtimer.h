#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QtTypes>

///! Measures the time between events.
/// ElapsedTimer measures time since its last restart, which is useful for
/// timing events that do not carry a timestamp of their own.
class ElapsedTimer : public QObject
{
    Q_OBJECT;
    QML_ELEMENT;

public:
    explicit ElapsedTimer();

    /// The seconds since the timer was last started or restarted, with
    /// nanosecond precision.
    Q_INVOKABLE qreal elapsed();

    /// Restart the timer, returning the seconds since it was last started
    /// or restarted.
    Q_INVOKABLE qreal restart();

    /// The milliseconds since the timer was last started or restarted.
    Q_INVOKABLE qint64 elapsedMs();

    /// Restart the timer, returning the milliseconds since it was last
    /// started or restarted.
    Q_INVOKABLE qint64 restartMs();

    /// The nanoseconds since the timer was last started or restarted.
    Q_INVOKABLE qint64 elapsedNs();

    /// Restart the timer, returning the nanoseconds since it was last
    /// started or restarted.
    Q_INVOKABLE qint64 restartNs();

private:
    QElapsedTimer timer;
};
