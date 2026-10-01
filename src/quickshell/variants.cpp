#include "variants.h"

#include <algorithm>
#include <utility>

#include <QQmlEngine>
#include <QQmlListProperty>

#include "reload.h"

namespace wqs {

void Variants::onReload(QObject *oldInstance)
{
    auto *old = qobject_cast<Variants *>(oldInstance);

    for (auto &[variant, instanceObj]: this->m_instances.values) {
        QObject *oldInstance = nullptr;
        if (old != nullptr) {
            auto &values = old->m_instances.values;

            if (variant.canConvert<QVariantMap>()) {
                auto variantMap = variant.value<QVariantMap>();

                int matchcount = 0;
                int matchi = 0;
                int i = 0;
                for (auto &[value, _]: values) {
                    if (!value.canConvert<QVariantMap>())
                        continue;
                    auto valueSet = value.value<QVariantMap>();

                    int count = 0;
                    for (auto [k, v]: variantMap.asKeyValueRange()) {
                        if (valueSet.contains(k) && valueSet.value(k) == v) {
                            count++;
                        }
                    }

                    if (count > matchcount) {
                        matchcount = count;
                        matchi = i;
                    }

                    i++;
                }

                if (matchcount > 0) {
                    oldInstance = values.takeAt(matchi).second;
                }
            } else {
                int i = 0;
                for (auto &[value, _]: values) {
                    if (variant == value) {
                        oldInstance = values.takeAt(i).second;
                        break;
                    }

                    i++;
                }
            }
        }

        auto *instance = qobject_cast<Reloadable *>(instanceObj);

        if (instance != nullptr)
            instance->reload(oldInstance);
        else
            Reloadable::reloadChildrenRecursive(instanceObj, oldInstance);
    }

    this->loaded = true;
}

QVariant Variants::model() const { return QVariant::fromValue(this->m_model); }

void Variants::setModel(const QVariant &model)
{
    if (model.canConvert<QVariantList>()) {
        this->m_model = model.value<QVariantList>();
    } else if (model.canConvert<QQmlListReference>()) {
        auto list = model.value<QQmlListReference>();
        if (!list.isReadable()) {
            qWarning() << "Non readable list" << model << "assigned to Variants.model, Ignoring.";
            return;
        }

        QVariantList model;
        auto size = list.count();
        for (auto i = 0; i < size; i++) {
            model.push_back(QVariant::fromValue(list.at(i)));
        }

        this->m_model = std::move(model);
    } else {
        qWarning() << "Non list data" << model << "assigned to Variants.model, Ignoring.";
        return;
    }

    this->updateVariants();
    emit this->modelChanged();
    emit this->instancesChanged();
}

QQmlListProperty<QObject> Variants::instances()
{
    return QQmlListProperty<QObject>(this, nullptr, &Variants::instanceCount,
                                     &Variants::instanceAt);
}

qsizetype Variants::instanceCount(QQmlListProperty<QObject> *prop)
{
    return static_cast<Variants *>(prop->object)->m_instances.values.length(); // NOLINT
}

QObject *Variants::instanceAt(QQmlListProperty<QObject> *prop, qsizetype i)
{
    return static_cast<Variants *>(prop->object)->m_instances.values.at(i).second; // NOLINT
}

void Variants::componentComplete()
{
    this->Reloadable::componentComplete();
    this->updateVariants();
}

void Variants::updateVariants()
{
    if (this->m_delegate == nullptr) {
        qWarning() << "Variants instance does not have a component specified";
        return;
    }

    // clean up removed entries
    for (auto iter = this->m_instances.values.begin();
         iter < this->m_instances.values.end();) {
        if (this->m_model.contains(iter->first)) {
            iter++;
        } else {
            iter->second->deleteLater();
            iter = this->m_instances.values.erase(iter);
        }
    }

    for (auto iter = this->m_model.begin(); iter < this->m_model.end(); iter++) {
        auto &variant = *iter;
        for (auto iter2 = this->m_model.begin(); iter2 < iter; iter2++) {
            if (*iter2 == variant) {
                qWarning() << "same value specified twice in Variants, duplicates will be"
                              "ignored:"
                           << variant;
                goto outer;
            }
        }

        {
            if (this->m_instances.contains(variant)) {
                continue; // we don't need to recreate this one
            }

            auto variantMap = QVariantMap();
            variantMap.insert("modelData", variant);

            auto *instance = this->m_delegate->createWithInitialProperties(
                variantMap, QQmlEngine::contextForObject(this->m_delegate));

            if (instance == nullptr) {
                qWarning() << this->m_delegate->errorString().toStdString().c_str();
                qWarning() << "failed to create variant with object" << variant;
                continue;
            }

            QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);

            instance->setParent(this);
            this->m_instances.insert(variant, instance);

            if (this->loaded) {
                if (auto *reloadable = qobject_cast<Reloadable *>(instance))
                    reloadable->reload(nullptr);
                else
                    Reloadable::reloadChildrenRecursive(instance, nullptr);
            }
        }

    outer:;
    }
}

template <typename K, typename V>
bool AwfulMap<K, V>::contains(const K &key) const
{
    return std::ranges::any_of(this->values, [&](const QPair<K, V> &pair) {
        return pair.first == key;
    });
}

template <typename K, typename V>
V *AwfulMap<K, V>::get(const K &key)
{
    for (auto &[k, v]: this->values) {
        if (key == k) {
            return &v;
        }
    }

    return nullptr;
}

template <typename K, typename V>
void AwfulMap<K, V>::insert(const K &key, V value)
{
    this->values.push_back(QPair<K, V>(key, value));
}

template <typename K, typename V>
bool AwfulMap<K, V>::remove(const K &key)
{
    for (auto iter = this->values.begin(); iter < this->values.end(); iter++) {
        if (iter->first == key) {
            this->values.erase(iter);

            return true;
        }
    }

    return false;
}

} // namespace wqs