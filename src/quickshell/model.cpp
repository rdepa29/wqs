#include "model.h"

#include <QByteArray>
#include <QHash>
#include <QObject>

QHash<int, QByteArray> UntypedObjectModel::roleNames() const { return {{Qt::UserRole, "modelData"}}; }

UntypedObjectModel *UntypedObjectModel::emptyInstance()
{
    static auto *instance = new ObjectModel<void>(nullptr);
    return instance;
}