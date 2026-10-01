#include "ShellRoot.h"

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

ShellRoot::ShellRoot(QObject *parent)
    : ReloadPropagator(parent)
    , m_settings(new QuickshellSettings(this))
{
}

} // namespace wqs