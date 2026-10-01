#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QVariant>
#include <QVariantList>

namespace wqs {

/// Port of Quickshell's `IpcHandler`. A config gives it a `target` and a `handle(message)`
/// method; calls arriving over `wqs ipc call <target> handle ...` are routed here.
class IpcHandler : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)

public:
    explicit IpcHandler(QObject *parent = nullptr);
    ~IpcHandler() override;

    QString target() const { return m_target; }
    void setTarget(const QString &target);

signals:
    void targetChanged();

private:
    void registerHandler();
    QVariant handle(const QVariantList &args, QString *error);

    QString m_target;
};

} // namespace wqs
