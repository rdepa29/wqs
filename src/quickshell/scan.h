#pragma once

#include <QDir>
#include <QHash>
#include <QVector>

namespace wqs {

///! Finds QML files under the config root and synthesizes the qmldir files Qt needs.
///
/// Qt requires a `qmldir` before it will resolve a module from an import path, but Caelestia and
/// most Quickshell configs just drop `.qml` files into folders. Upstream walks the config tree
/// while following imports and synthesizes a qmldir for every directory it visits, marking the
/// types whose file declares `pragma Singleton`.
///
/// Upstream also runs the QML preprocessor here, which needs a JS engine and a pragma parser.
/// wqs has no preprocessor yet, so this walks the whole config root up front instead of
/// following imports, and only does the part module resolution needs.
class QmlScanner
{
public:
    QmlScanner() = default;
    explicit QmlScanner(const QDir &rootPath) : m_rootPath(rootPath) {}

    void scan();

    QVector<QDir> scannedDirs;
    QVector<QString> scannedFiles;
    QHash<QString, QString> fileIntercepts;

private:
    void scanDir(const QDir &dir);
    static bool scanQmlFile(const QString &path, bool *singleton, QStringList *scannedFiles);

    QDir m_rootPath;
};

} // namespace wqs