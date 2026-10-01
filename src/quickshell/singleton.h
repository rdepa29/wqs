#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

#include "reload.h"

namespace wqs {

///! The root component for reloadable singletons.
/// All singletons should inherit from this type.
class Singleton : public ReloadPropagator
{
    Q_OBJECT
    QML_ELEMENT

public:
    void componentComplete() override;
};

class SingletonRegistry
{
public:
    SingletonRegistry() = default;

    void registerSingleton(const QUrl &url, Singleton *singleton);
    void onReload(SingletonRegistry *old);

private:
    QHash<QUrl, Singleton *> registry;
};

} // namespace wqs