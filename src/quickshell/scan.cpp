#include "scan.h"

#include <QFile>
#include <QLoggingCategory>
#include <QTextStream>

Q_LOGGING_CATEGORY(logQmlScanner, "wqs.scanner", QtWarningMsg)

namespace wqs {

void QmlScanner::scan()
{
    if (!this->m_rootPath.exists())
        return;

    this->scanDir(this->m_rootPath);
}

void QmlScanner::scanDir(const QDir &dir)
{
    if (this->scannedDirs.contains(dir))
        return;
    this->scannedDirs.push_back(dir);

    const auto &path = dir.path();

    qCDebug(logQmlScanner) << "Scanning directory" << path;

    struct Entry {
        QString name;
        bool singleton = false;
    };

    bool seenQmldir = false;
    auto entries = QVector<Entry>();

    for (auto &name: dir.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (name == "qmldir") {
            qCDebug(logQmlScanner)
                << "Found qmldir file, qmldir synthesization will be disabled for directory" << path;
            seenQmldir = true;
        } else if (name.at(0).isUpper() && name.endsWith(".qml")) {
            auto &entry = entries.emplaceBack();

            if (QmlScanner::scanQmlFile(dir.filePath(name), &entry.singleton, &this->scannedFiles)) {
                entry.name = name;
            } else {
                entries.pop_back();
            }
        }
    }

    if (!seenQmldir) {
        qCDebug(logQmlScanner) << "Synthesizing qmldir for directory" << path;

        QString qmldir;
        auto stream = QTextStream(&qmldir);

        // can't derive a module name if not in shell path
        if (path.startsWith(this->m_rootPath.path())) {
            auto end = path.sliced(this->m_rootPath.path().length());

            // verify we have a valid module name.
            bool valid = true;
            for (auto &c: end) {
                if (c == '/') {
                    c = '.';
                } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                           || (c >= '0' && c <= '9') || c == '_')
                {
                } else {
                    qCWarning(logQmlScanner)
                        << "Module path contains invalid characters for a module name:"
                        << path.sliced(this->m_rootPath.path().length());
                    valid = false;
                    break;
                }
            }

            if (valid)
                stream << "module qs" << end << '\n';
        } else {
            qCWarning(logQmlScanner) << "Module path" << path << "is outside of the config folder.";
        }

        for (const auto &entry: entries) {
            if (entry.singleton)
                stream << "singleton ";
            stream << entry.name.sliced(0, entry.name.length() - 4) << " 1.0 " << entry.name
                   << '\n';
        }

        qCDebug(logQmlScanner) << "Synthesized qmldir for" << path << qPrintable("\n" + qmldir);
        this->fileIntercepts.insert(QDir(path).filePath("qmldir"), qmldir);
    }

    for (auto &sub: dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        this->scanDir(QDir(dir.filePath(sub)));
    }
}

bool QmlScanner::scanQmlFile(const QString &path, bool *singleton, QStringList *scannedFiles)
{
    if (scannedFiles->contains(path))
        return false;
    scannedFiles->append(path);

    qCDebug(logQmlScanner) << "Scanning qml file" << path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(logQmlScanner) << "Failed to open file" << path;
        return false;
    }

    QTextStream stream(&file);

    // Qt only honours `pragma Singleton` in the file header, before any real statement
    while (!stream.atEnd()) {
        auto line = stream.readLine().trimmed();

        if (line == QLatin1String("pragma Singleton")) {
            *singleton = true;
        } else if (!line.isEmpty() && !line.startsWith(QLatin1String("//"))
                   && !line.startsWith(QLatin1String("import"))
                   && !line.startsWith(QLatin1String("pragma")))
        {
            break;
        }
    }

    return true;
}

} // namespace wqs