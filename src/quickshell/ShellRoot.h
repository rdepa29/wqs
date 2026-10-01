#pragma once

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

#include "reload.h"

namespace wqs {

///! Accessor for some options under the Quickshell type.
class QuickshellSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(QuickshellSettings)
    QML_UNCREATABLE("QuickshellSettings is provided by ShellRoot")

    Q_PROPERTY(bool watchFiles READ watchFiles WRITE setWatchFiles NOTIFY watchFilesChanged)
    Q_PROPERTY(bool reloadPopup READ reloadPopup WRITE setReloadPopup NOTIFY reloadPopupChanged)

public:
    explicit QuickshellSettings(QObject *parent = nullptr);

    bool watchFiles() const { return m_watchFiles; }
    void setWatchFiles(bool watch);
    bool reloadPopup() const { return m_reloadPopup; }
    void setReloadPopup(bool popup);

signals:
    void watchFilesChanged();
    void reloadPopupChanged();

private:
    bool m_watchFiles = true;
    bool m_reloadPopup = true;
};

///! Optional root config element, allowing some settings to be specified inline.
class ShellRoot : public ReloadPropagator
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QuickshellSettings *settings READ settings CONSTANT)

public:
    explicit ShellRoot(QObject *parent = nullptr);

    QuickshellSettings *settings() const { return m_settings; }

private:
    QuickshellSettings *m_settings = nullptr;
};

} // namespace wqs