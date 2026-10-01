#include "Scope.h"

namespace wqs {

QuickshellSettings::QuickshellSettings(QObject *parent)
    : QObject(parent)
{
}

void QuickshellSettings::setWatchFiles(bool watch)
{
    if (m_watchFiles == watch)
        return;
    m_watchFiles = watch;
    emit watchFilesChanged();
}

void QuickshellSettings::setReloadPopup(bool popup)
{
    if (m_reloadPopup == popup)
        return;
    m_reloadPopup = popup;
    emit reloadPopupChanged();
}

Scope::Scope(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QObject> Scope::children()
{
    return QQmlListProperty<QObject>(this, nullptr, &Scope::appendChild, &Scope::childCount,
                                     &Scope::childAt, &Scope::clearChildren);
}

void Scope::appendChild(QQmlListProperty<QObject> *prop, QObject *object)
{
    auto *scope = qobject_cast<Scope *>(prop->object);
    if (!scope || !object)
        return;
    object->setParent(scope);
    scope->m_children.append(object);
}

qsizetype Scope::childCount(QQmlListProperty<QObject> *prop)
{
    auto *scope = static_cast<Scope *>(prop->object);
    return scope ? scope->m_children.size() : 0;
}

QObject *Scope::childAt(QQmlListProperty<QObject> *prop, qsizetype index)
{
    auto *scope = static_cast<Scope *>(prop->object);
    if (!scope || index < 0 || index >= scope->m_children.size())
        return nullptr;
    return scope->m_children.at(index);
}

void Scope::clearChildren(QQmlListProperty<QObject> *prop)
{
    auto *scope = static_cast<Scope *>(prop->object);
    if (!scope)
        return;
    for (QObject *child : scope->m_children)
        child->deleteLater();
    scope->m_children.clear();
}

ShellRoot::ShellRoot(QObject *parent)
    : Scope(parent)
    , m_settings(new QuickshellSettings(this))
{
}

} // namespace wqs
