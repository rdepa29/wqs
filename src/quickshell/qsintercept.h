#pragma once

#include <QByteArray>
#include <QDir>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QtQml/qqmlabstracturlinterceptor.h>
#include <QtQml/qqmlnetworkaccessmanagerfactory.h>

namespace wqs {

///! Maps Quickshell's `qs:` URL scheme onto the config directory.
class QsUrlInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    explicit QsUrlInterceptor(const QDir &configRoot);

    QUrl intercept(const QUrl &originalUrl, QQmlAbstractUrlInterceptor::DataType type) override;

private:
    QDir m_configRoot;
};

///! Serves in-memory file contents over the network access manager.
///
/// The qmldir files QmlScanner synthesizes do not exist on disk, so they have to be answered from
/// memory. This also covers the qmldir file interception path, where the interceptor hands back a
/// `qs:` URL that must still resolve to a real file.
class QsInterceptDataReply : public QNetworkReply
{
    Q_OBJECT

public:
    QsInterceptDataReply(const QNetworkRequest &request, const QByteArray &data, QObject *parent);
    ~QsInterceptDataReply() override;

    qint64 bytesAvailable() const override;
    bool isSequential() const override;
    void abort() override;
    qint64 readData(char *data, qint64 maxSize) override;

private:
    QByteArray m_data;
    qint64 m_offset = 0;
};

///! Serves `qs:` URLs from the config directory.
///
/// Qt loads a QML file whose URL is not `file:`/`qrc:` through the network access manager, so the
/// interceptor alone cannot make `qs:` work - this is what actually reads the files off disk.
class QsInterceptNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    QsInterceptNetworkAccessManager(const QDir &configRoot,
                                    const QHash<QString, QString> &fileIntercepts,
                                    QObject *parent = nullptr);

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req,
                                 QIODevice *outgoingData = nullptr) override;

private:
    QDir m_configRoot;
    const QHash<QString, QString> &m_fileIntercepts;
};

class QsInterceptNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QsInterceptNetworkAccessManagerFactory(const QDir &configRoot,
                                           const QHash<QString, QString> &fileIntercepts)
        : m_configRoot(configRoot)
        , m_fileIntercepts(fileIntercepts)
    {
    }

    QNetworkAccessManager *create(QObject *parent) override;

private:
    QDir m_configRoot;
    const QHash<QString, QString> &m_fileIntercepts;
};

} // namespace wqs