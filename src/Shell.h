#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#ifndef WQS_VERSION
#define WQS_VERSION "0.1.0"
#endif

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

    bool hasMultipleScreens() const { return screens().size() > 1; }
    // primary first, then the rest by position
    QStringList screens() const;

    Q_INVOKABLE QString configPath() const;
    Q_INVOKABLE QString dataPath() const;
    Q_INVOKABLE QString logPath() const;

    struct Source
    {
        QUrl url;
        bool isResource = false;
    };

    // -p/--path, then -c/--config, then default, then bundled; empty on failure
    static Source resolveSource(const QString &pathOpt, const QString &configOpt,
                                QString *error);

signals:
    void screensChanged();

private:
    void refreshScreens();

    QStringList m_screens;
};