#include "qsintercept.h"

#include <QBuffer>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>
#include <cstring>

Q_LOGGING_CATEGORY(logQsIntercept, "wqs.interceptor", QtWarningMsg)

namespace wqs {

QsUrlInterceptor::QsUrlInterceptor(const QDir &configRoot)
    : m_configRoot(configRoot)
{
}

QUrl QsUrlInterceptor::intercept(const QUrl &originalUrl,
                                 QQmlAbstractUrlInterceptor::DataType type)
{
    auto url = originalUrl;

    if (url.scheme() == "root") {
        url.setScheme("qs");

        auto path = url.path();
        if (path.startsWith('/'))
            path = path.sliced(1);
        url.setPath("@/qs/" % path);

        qCDebug(logQsIntercept) << "Rewrote root intercept" << originalUrl << "to" << url;
    }

    if (url.scheme() == "qs") {
        auto path = url.path();

        // Our import path is on "qs:@/".
        // We want to blackhole any import resolution outside of the config folder as it breaks
        // Qt but NOT file lookups that might be on "qs:/" due to a missing "file:/" prefix.
        if (path.startsWith("@/qs/")) {
            path = this->m_configRoot.filePath(path.sliced(5));
        } else if (!path.startsWith("/")) {
            qCDebug(logQsIntercept) << "Blackholed import URL" << url;
            return QUrl("qrc:/qs-blackhole");
        }

        // Some types such as Image take into account where they are loading from, and force
        // asynchronous loading over a network. qs: is considered to be over a network.
        // In those cases we want to return a file:// url so asynchronous loading is not forced.
        if (type == QQmlAbstractUrlInterceptor::DataType::UrlString) {
            // Qt.resolvedUrl and context->resolvedUrl can use this on qml files, in which
            // case we want to keep the intercept, otherwise objects created from those paths
            // will not be able to use singletons.
            if (path.endsWith(".qml"))
                return url;

            // fromLocalFile, because setting scheme+path on the qs: url would drop the authority
            auto newUrl = QUrl::fromLocalFile(path);
            qCDebug(logQsIntercept) << "Rewrote intercept" << url << "to" << newUrl;
            return newUrl;
        }

        // QML files must be loaded synchronously, since QQmlComponent::create() does not wait for
        // the network access manager to finish. Upstream gets away with handing out qs: urls here
        // because the network access manager below answers them, but a still-pending reply leaves
        // the component in the Loading state. The files are on disk unmodified, so hand out
        // file: urls and let Qt read them directly.
        if (type == QQmlAbstractUrlInterceptor::DataType::QmlFile) {
            auto newUrl = QUrl::fromLocalFile(path);
            qCDebug(logQsIntercept) << "Rewrote qml file intercept" << url << "to" << newUrl;
            return newUrl;
        }

        // qmldir files keep the qs: scheme, since the ones QmlScanner synthesizes only exist in
        // memory and have to be answered by the network access manager.
    }

    return url;
}

QsInterceptDataReply::QsInterceptDataReply(const QNetworkRequest &request, const QByteArray &data,
                                           QObject *parent)
    : QNetworkReply(parent)
    , m_data(data)
{
    this->setRequest(request);
    // QIODevice::read on the reply itself has to see an open device
    this->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    this->setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
    this->setHeader(QNetworkRequest::ContentLengthHeader, data.size());

    // Report the reply as already finished. QQmlDataBlob only takes its synchronous path when
    // the URL is local or the reply is finished, and QQmlComponent::create() does not wait for a
    // pending one - the component would stay in the Loading state and fail to instantiate.
    this->setFinished(true);

    // still notify late connecters, in case a consumer attaches after the fact
    QMetaObject::invokeMethod(this, &QNetworkReply::metaDataChanged, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, &QNetworkReply::finished, Qt::QueuedConnection);
}

QsInterceptDataReply::~QsInterceptDataReply() = default;

qint64 QsInterceptDataReply::bytesAvailable() const
{
    return this->m_data.size() - this->m_offset + QIODevice::bytesAvailable();
}

bool QsInterceptDataReply::isSequential() const
{
    return true;
}

void QsInterceptDataReply::abort()
{
    this->setError(QNetworkReply::OperationCanceledError, QStringLiteral("Operation canceled"));
    this->close();
    this->setFinished(true);
}

qint64 QsInterceptDataReply::readData(char *data, qint64 maxSize)
{
    auto len = std::min(maxSize, this->m_data.size() - this->m_offset);
    if (len <= 0)
        return 0;

    std::memcpy(data, this->m_data.constData() + this->m_offset, len);
    this->m_offset += len;
    return len;
}

QsInterceptNetworkAccessManager::QsInterceptNetworkAccessManager(
    const QDir &configRoot, const QHash<QString, QString> &fileIntercepts, QObject *parent)
    : QNetworkAccessManager(parent)
    , m_configRoot(configRoot)
    , m_fileIntercepts(fileIntercepts)
{
}

QNetworkReply *QsInterceptNetworkAccessManager::createRequest(Operation op,
                                                              const QNetworkRequest &req,
                                                              QIODevice *outgoingData)
{
    auto url = req.url();

    if (url.scheme() == "qs") {
        auto path = url.path();

        if (path.startsWith("@/qs/")) {
            path = this->m_configRoot.filePath(path.sliced(5));
        }
        // otherwise pass through to fs

        auto it = this->m_fileIntercepts.constFind(path);
        if (it != this->m_fileIntercepts.constEnd()) {
            qCDebug(logQsIntercept) << "Intercepting request to" << path;
            return new QsInterceptDataReply(req, it.value().toUtf8(), this);
        }

        qCDebug(logQsIntercept) << "Passing through intercept" << url << "to" << path;

        auto fileReq = req;
        fileReq.setUrl(QUrl::fromLocalFile(path));
        return this->QNetworkAccessManager::createRequest(op, fileReq, outgoingData);
    }

    return this->QNetworkAccessManager::createRequest(op, req, outgoingData);
}

QNetworkAccessManager *QsInterceptNetworkAccessManagerFactory::create(QObject *parent)
{
    return new QsInterceptNetworkAccessManager(this->m_configRoot, this->m_fileIntercepts, parent);
}

} // namespace wqs