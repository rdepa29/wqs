#pragma once

#include <QObject>
#include <QQmlIncubator>

///! Incubator wrapper that turns QQmlIncubator's status enum into signals.
class QsQmlIncubator : public QObject, public QQmlIncubator
{
    Q_OBJECT

public:
    explicit QsQmlIncubator(QsQmlIncubator::IncubationMode mode, QObject *parent = nullptr)
        : QObject(parent)
        , QQmlIncubator(mode)
    {
    }

    void statusChanged(QQmlIncubator::Status status) override;

signals:
    void completed();
    void failed();
};

///! Incubates objects asynchronously so creation can be spread over multiple frames.
///
/// Upstream schedules incubation from the gaps between Qt Quick render loop frames, which needs
/// the private QSGRenderLoop API. Qt 6.8 has no such hook, and drives asynchronous incubation
/// from the engine's own timer instead, so this is currently a hook point only.
class QsIncubationController : public QObject, public QQmlIncubationController
{
    Q_OBJECT

protected:
    void incubatingObjectCountChanged(int count) override;
};