#pragma once

#include <QDir>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>

#include "incubator.h"
#include "qsintercept.h"
#include "scan.h"
#include "singleton.h"

namespace wqs {

///! Owns a QQmlEngine and the loaded config, and drives the reload sequence.
///
/// Upstream's EngineGeneration also carries the image providers and plugin hooks. wqs does not
/// have those yet, so this only covers what the reloadable QML types need: an engine ->
/// generation lookup, the singleton registry, and the reload signals.
class EngineGeneration : public QObject
{
    Q_OBJECT

public:
    explicit EngineGeneration(QQmlEngine *engine, const QDir &configRoot);
    ~EngineGeneration() override;

    // Loads the entry file as this generation's root and runs the initial reload pass.
    bool load(const QUrl &url);
    // Reuses state from `old` in this generation's root, then drops the old generation.
    void onReload(EngineGeneration *old);
    // Drops the loaded root without touching the engine.
    void destroy();

    static EngineGeneration *findEngineGeneration(const QQmlEngine *engine);
    static EngineGeneration *findObjectGeneration(const QObject *object);

    // Returns the current generation if there is only one generation, otherwise null.
    static EngineGeneration *currentGeneration();

    QQmlEngine *engine = nullptr;
    QObject *root = nullptr;
    QDir rootPath;
    QmlScanner scanner;
    SingletonRegistry singletonRegistry;
    QsIncubationController incubationController;
    QsUrlInterceptor urlInterceptor;
    QsInterceptNetworkAccessManagerFactory interceptNetFactory;
    bool reloadComplete = false;

signals:
    void reloadFinished();
    void firePostReload();

public slots:
    void quit();
    void exit(int code);

private:
    void completeReload(EngineGeneration *old);
    void postReload();
};

} // namespace wqs