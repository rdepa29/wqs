#include "generation.h"

#include <QCoreApplication>
#include <QHash>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

#include "reload.h"

namespace {
QHash<const QQmlEngine *, wqs::EngineGeneration *> g_generations;
}

namespace wqs {

EngineGeneration::EngineGeneration(QQmlEngine *engine)
    : QObject(nullptr)
    , engine(engine)
{
    g_generations.insert(engine, this);
    this->engine->setIncubationController(&this->incubationController);

    QObject::connect(this->engine, &QQmlEngine::quit, this, &EngineGeneration::quit);
    QObject::connect(this->engine, &QQmlEngine::exit, this, &EngineGeneration::exit);
}

void EngineGeneration::quit() { QCoreApplication::quit(); }

void EngineGeneration::exit(int code) { QCoreApplication::exit(code); }

EngineGeneration::~EngineGeneration()
{
    if (this->engine != nullptr)
        g_generations.remove(this->engine);
}

bool EngineGeneration::load(const QUrl &url)
{
    auto *component = new QQmlComponent(this->engine, url);
    auto *object = component->create();

    if (object == nullptr) {
        qCritical().noquote() << "wqs: failed to load" << url.toString() << "-"
                              << component->errorString().trimmed();
        delete component;
        return false;
    }

    delete component;

    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
    this->root = object;
    this->completeReload(nullptr);
    return true;
}

void EngineGeneration::onReload(EngineGeneration *old) { this->completeReload(old); }

void EngineGeneration::destroy()
{
    if (this->root != nullptr) {
        this->root->deleteLater();
        this->root = nullptr;
    }
}

void EngineGeneration::completeReload(EngineGeneration *old)
{
    if (auto *reloadable = qobject_cast<Reloadable *>(this->root))
        reloadable->reload(old == nullptr ? nullptr : old->root);

    this->singletonRegistry.onReload(old == nullptr ? nullptr : &old->singletonRegistry);
    this->reloadComplete = true;
    emit this->reloadFinished();

    if (old != nullptr) {
        old->destroy();
        old->deleteLater();
    }

    this->postReload();
}

void EngineGeneration::postReload()
{
    // This can be called on a generation during its destruction.
    if (this->engine == nullptr || this->root == nullptr)
        return;

    emit this->firePostReload();
    QObject::disconnect(this, &EngineGeneration::firePostReload, nullptr, nullptr);
}

EngineGeneration *EngineGeneration::findEngineGeneration(const QQmlEngine *engine)
{
    return g_generations.value(engine);
}

EngineGeneration *EngineGeneration::findObjectGeneration(const QObject *object)
{
    // Objects can still attempt to find their generation after it has been destroyed.
    while (object != nullptr) {
        auto *context = QQmlEngine::contextForObject(object);

        if (context != nullptr) {
            if (auto *generation = EngineGeneration::findEngineGeneration(context->engine())) {
                return generation;
            }
        }

        object = object->parent();
    }

    return nullptr;
}

EngineGeneration *EngineGeneration::currentGeneration()
{
    if (g_generations.size() == 1) {
        return *g_generations.begin();
    } else
        return nullptr;
}

} // namespace wqs