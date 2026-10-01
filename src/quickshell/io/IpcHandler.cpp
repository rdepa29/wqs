#include "IpcHandler.h"

#include <QMetaObject>
#include <QMetaMethod>

#include "../../IpcServer.h"

namespace wqs {

IpcHandler::IpcHandler(QObject *parent)
    : QObject(parent)
{
}

IpcHandler::~IpcHandler()
{
    if (IpcServer *server = IpcServer::instance(); server && !m_target.isEmpty())
        server->unregisterHandler(m_target, QStringLiteral("handle"));
}

void IpcHandler::setTarget(const QString &target)
{
    if (m_target == target)
        return;
    if (IpcServer *server = IpcServer::instance(); server && !m_target.isEmpty())
        server->unregisterHandler(m_target, QStringLiteral("handle"));

    m_target = target;
    emit targetChanged();
    registerHandler();
}

void IpcHandler::registerHandler()
{
    IpcServer *server = IpcServer::instance();
    if (!server || m_target.isEmpty())
        return;

    server->registerHandler(m_target, QStringLiteral("handle"),
                            [this](const QVariantList &args, QString *error) -> QVariant {
                                return handle(args, error);
                            });
}

QVariant IpcHandler::handle(const QVariantList &args, QString *error)
{
    QStringList parts;
    parts.reserve(args.size());
    for (const QVariant &arg : args)
        parts << arg.toString();
    const QString message = parts.join(QLatin1Char(' '));

    // A QML `handle(message)` may be declared with or without type annotations, so its
    // parameter and return types are discovered from the meta-object rather than assumed.
    const QMetaObject *meta = metaObject();
    for (int i = 0; i < meta->methodCount(); ++i) {
        const QMetaMethod method = meta->method(i);
        if (method.name() != QByteArrayLiteral("handle"))
            continue;

        const QByteArray returnType = method.typeName();
        const bool hasArg = method.parameterCount() == 1;
        const bool argIsString = hasArg && method.parameterType(0) == QMetaType::QString;
        const bool argIsVariant = hasArg && method.parameterType(0) == QMetaType::QVariant;
        if (!argIsString && !argIsVariant && !method.parameterCount())
            continue;

        QVariant result;
        bool ok = false;
        if (method.parameterCount() == 0) {
            if (returnType == QByteArrayLiteral("QString")) {
                QString s;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QString, s));
                if (ok)
                    result = s;
            } else {
                QVariant v;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QVariant, v));
                if (ok)
                    result = v;
            }
        } else if (argIsString) {
            if (returnType == QByteArrayLiteral("QString")) {
                QString s;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QString, s),
                                   Q_ARG(QString, message));
                if (ok)
                    result = s;
            } else {
                QVariant v;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QVariant, v),
                                   Q_ARG(QString, message));
                if (ok)
                    result = v;
            }
        } else {
            QVariant msg(message);
            if (returnType == QByteArrayLiteral("QString")) {
                QString s;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QString, s),
                                   Q_ARG(QVariant, msg));
                if (ok)
                    result = s;
            } else {
                QVariant v;
                ok = method.invoke(this, Qt::DirectConnection, Q_RETURN_ARG(QVariant, v),
                                   Q_ARG(QVariant, msg));
                if (ok)
                    result = v;
            }
        }
        if (ok)
            return result;
    }

    if (error)
        *error = QStringLiteral("IpcHandler '%1' has no callable handle(message) function")
                     .arg(m_target);
    return {};
}

} // namespace wqs
