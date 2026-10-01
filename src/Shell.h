#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#ifndef WQS_VERSION
#define WQS_VERSION "0.1.0"
#endif

/// The `wqs` root context object. Quickshell's ShellRoot owns the whole shell and exposes
/// every piece of configuration from it; on Windows most of those hooks are handled by the
/// window manager, so this class keeps the lighter, still-needed ones: paths, version,
/// logging, and a list of usable screens.
class Shell : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool hasMultipleScreens READ hasMultipleScreens CONSTANT)
    Q_PROPERTY(QStringList screens READ screens NOTIFY screensChanged)

public:
    explicit Shell(QObject *parent = nullptr);
    ~Shell() override;

    QString version() const { return QStringLiteral(WQS_VERSION); }

    /// Primary screen first, then every other one ordered by position from the top-left.
    bool hasMultipleScreens() const { return screens().size() > 1; }
    QStringList screens() const;

    Q_INVOKABLE QString configPath() const;
    Q_INVOKABLE QString dataPath() const;
    Q_INVOKABLE QString logPath() const;

    /// A resolved entry point: which shell.qml to load, and whether it came from the
    /// bundled resource instead of the user's config directory.
    struct Source
    {
        QUrl url;
        bool isResource = false;
    };

    /// Resolves the shell.qml to run, in Quickshell's `qs` order: an explicit `-p/--path`
    /// (file or directory), then `-c/--config <name>` under the config dir, then the
    /// user's default config, and finally the shell bundled into the executable. Returns
    /// an invalid Source and fills `error` when an explicit option points at nothing.
    static Source resolveSource(const QString &pathOpt, const QString &configOpt,
                                QString *error);

signals:
    void screensChanged();

private:
    void refreshScreens();

    QStringList m_screens;
};