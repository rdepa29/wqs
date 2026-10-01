#include "lazyloader.h"

#include <utility>

#include <QQmlContext>
#include <QQmlEngine>

#include "reload.h"

namespace wqs {

void LazyLoader::onReload(QObject *oldInstance)
{
    auto *old = qobject_cast<LazyLoader *>(oldInstance);

    this->incubateIfReady(true);

    if (old != nullptr && old->m_item != nullptr && this->incubator != nullptr) {
        this->incubator->forceCompletion();
    }

    if (this->m_item != nullptr) {
        if (auto *reloadable = qobject_cast<Reloadable *>(this->m_item)) {
            reloadable->reload(old == nullptr ? nullptr : old->m_item);
        } else {
            Reloadable::reloadRecursive(this->m_item, old);
        }
    }
}

QObject *LazyLoader::item()
{
    if (this->isLoading())
        this->setActive(true);
    return this->m_item;
}

void LazyLoader::setItem(QObject *item)
{
    if (item == this->m_item)
        return;

    if (this->m_item != nullptr) {
        this->m_item->deleteLater();
    }

    this->m_item = item;

    if (item != nullptr) {
        item->setParent(this);
    }

    this->targetActive = this->isActive();

    emit this->itemChanged();
    emit this->activeChanged();
}

bool LazyLoader::isLoading() const { return this->incubator != nullptr; }

void LazyLoader::setLoading(bool loading)
{
    if (loading == this->targetLoading || this->isActive())
        return;
    this->targetLoading = loading;

    if (loading) {
        this->incubateIfReady();
    } else if (this->m_item != nullptr) {
        this->m_item->deleteLater();
        this->m_item = nullptr;
    } else if (this->incubator != nullptr) {
        delete this->incubator;
        this->incubator = nullptr;
    }
}

bool LazyLoader::isActive() const { return this->m_item != nullptr; }

void LazyLoader::setActive(bool active)
{
    if (active == this->targetActive)
        return;
    this->targetActive = active;

    if (active) {
        if (this->isLoading()) {
            this->incubator->forceCompletion();
        } else if (!this->isActive()) {
            this->incubateIfReady();
        }
    } else if (this->isActive()) {
        this->setItem(nullptr);
    }
}

void LazyLoader::setActiveAsync(bool active)
{
    if (active == (this->targetActive || this->targetLoading))
        return;
    if (active)
        this->setLoading(true);
    else
        this->setActive(false);
}

QQmlComponent *LazyLoader::component() const
{
    return this->cleanupComponent ? nullptr : this->m_component;
}

void LazyLoader::setComponent(QQmlComponent *component)
{
    if (this->cleanupComponent)
        this->setSource(nullptr);
    if (component == this->m_component)
        return;
    this->cleanupComponent = false;

    if (this->m_component != nullptr) {
        QObject::disconnect(this->m_component, nullptr, this, nullptr);
    }

    this->m_component = component;

    if (component != nullptr) {
        QObject::connect(this->m_component, &QObject::destroyed, this,
                         &LazyLoader::onComponentDestroyed);
    }

    emit this->componentChanged();
}

void LazyLoader::onComponentDestroyed()
{
    this->m_component = nullptr;
    // todo: figure out what happens to the incubator
}

QString LazyLoader::source() const { return this->m_source; }

void LazyLoader::setSource(QString source)
{
    if (!this->cleanupComponent)
        this->setComponent(nullptr);
    if (source == this->m_source)
        return;
    this->cleanupComponent = true;

    this->m_source = std::move(source);
    delete this->m_component;

    if (!this->m_source.isEmpty()) {
        auto *context = QQmlEngine::contextForObject(this);
        this->m_component =
            new QQmlComponent(context == nullptr ? nullptr : context->engine(),
                              context == nullptr ? this->m_source : context->resolvedUrl(this->m_source));

        if (this->m_component->isError()) {
            qWarning() << this->m_component->errorString().toStdString().c_str();
            delete this->m_component;
            this->m_component = nullptr;
        }
    } else {
        this->m_component = nullptr;
    }

    emit this->sourceChanged();
}

void LazyLoader::incubateIfReady(bool overrideReloadCheck)
{
    if (!(this->reloadComplete || overrideReloadCheck)
        || !(this->targetLoading || this->targetActive) || this->m_component == nullptr
        || this->incubator != nullptr)
    {
        return;
    }

    this->incubator = new QsQmlIncubator(
        this->targetActive ? QQmlIncubator::Synchronous : QQmlIncubator::Asynchronous, this);

    QObject::connect(this->incubator, &QsQmlIncubator::completed, this,
                     &LazyLoader::onIncubationCompleted);
    QObject::connect(this->incubator, &QsQmlIncubator::failed, this,
                     &LazyLoader::onIncubationFailed);

    emit this->loadingChanged();

    this->m_component->create(*this->incubator, QQmlEngine::contextForObject(this->m_component));
}

void LazyLoader::onIncubationCompleted()
{
    this->setItem(this->incubator->object());
    // The incubator is not necessarily inert at the time of this callback,
    // so deleteLater is required.
    this->incubator->deleteLater();
    this->incubator = nullptr;
    this->targetLoading = false;
    emit this->loadingChanged();
}

void LazyLoader::onIncubationFailed()
{
    qWarning() << "Failed to create LazyLoader component";

    for (auto &error: this->incubator->errors()) {
        qWarning() << error;
    }

    delete this->incubator;
    this->targetLoading = false;
    emit this->loadingChanged();
}

} // namespace wqs