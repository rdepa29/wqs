#include "desktopentry.h"

#include <algorithm>
#include <utility>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QProperty>
#include <QScopeGuard>
#include <QThreadPool>
#include <ranges>

#include "Quickshell.h"

Q_LOGGING_CATEGORY(logDesktopEntry, "wqs.desktopentry", QtWarningMsg);

#ifdef Q_OS_WIN
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>

#    include <objbase.h>
#    include <shobjidl.h>
#endif

namespace {
struct Locale {
    explicit Locale() = default;

    explicit Locale(const QString &string)
    {
        auto territoryIdx = string.indexOf('_');
        auto codesetIdx = string.indexOf('.');
        auto modifierIdx = string.indexOf('@');

        auto parseEnd = string.length();

        if (modifierIdx != -1) {
            this->modifier = string.sliced(modifierIdx + 1, parseEnd - modifierIdx - 1);
            parseEnd = modifierIdx;
        }

        if (codesetIdx != -1) {
            parseEnd = codesetIdx;
        }

        if (territoryIdx != -1) {
            this->territory = string.sliced(territoryIdx + 1, parseEnd - territoryIdx - 1);
            parseEnd = territoryIdx;
        }

        this->language = string.sliced(0, parseEnd);
    }

    [[nodiscard]] bool isValid() const { return !this->language.isEmpty(); }

    [[nodiscard]] int matchScore(const Locale &other) const
    {
        if (this->language != other.language)
            return 0;

        if (!other.modifier.isEmpty() && this->modifier != other.modifier)
            return 0;
        if (!other.territory.isEmpty() && this->territory != other.territory)
            return 0;

        auto score = 1;

        if (!other.territory.isEmpty())
            score += 2;
        if (!other.modifier.isEmpty())
            score += 1;

        return score;
    }

    static const Locale &system()
    {
        static Locale *locale = nullptr; // NOLINT

        if (locale == nullptr) {
            auto lstr = qEnvironmentVariable("LC_MESSAGES");
            if (lstr.isEmpty())
                lstr = qEnvironmentVariable("LANG");
            locale = new Locale(lstr);
        }

        return *locale;
    }

    QString language;
    QString territory;
    QString modifier;
};

// NOLINTNEXTLINE(misc-use-internal-linkage)
QDebug operator<<(QDebug debug, const Locale &locale)
{
    auto saver = QDebugStateSaver(debug);
    debug.nospace() << "Locale(language=" << locale.language << ", territory=" << locale.territory
                    << ", modifier=" << locale.modifier << ')';

    return debug;
}

#ifdef Q_OS_WIN
/// Expand `%NAME%` references in a shortcut target or argument string.
///
/// Qt has no public helper for this on Windows, and shortcuts may store targets such
/// as `%windir%\system32\cleanmgr.exe`.
QString expandEnvironment(const QString &input)
{
    static const auto environment = []() {
        auto vars = QHash<QString, QString>();
        const auto system = QProcessEnvironment::systemEnvironment();
        for (const auto &key: system.keys())
            vars.insert(key, system.value(key));
        return vars;
    }();

    QString out;
    out.reserve(input.size());

    for (qsizetype i = 0; i < input.size(); i++) {
        if (input.at(i) != u'%') {
            out += input.at(i);
            continue;
        }

        auto end = input.indexOf(u'%', i + 1);
        if (end == -1) {
            out += input.mid(i);
            break;
        }

        auto name = input.sliced(i + 1, end - i - 1);
        auto value = environment.value(name.toUpper());
        if (value.isEmpty()) {
            // unknown variable; keep the reference as-is
            out += input.sliced(i, end - i + 1);
        } else {
            out += value;
        }

        i = end;
    }

    return out;
}

/// The IID constants are pointers under MinGW and objects under MSVC; normalize them.
template <typename TId>
const IID *resolveIid(const TId &id)
{
    return &id;
}

template <typename TId>
const IID *resolveIid(const TId *id)
{
    return id;
}

/// Resolve a Start Menu shortcut into entry data.
///
/// Windows has no desktop entry specification, so the Start Menu (.lnk) is the closest
/// equivalent: the shortcut's target becomes the `Exec` command, and its icon location
/// becomes the entry icon. Only the fields a launcher can meaningfully use are populated.
ParsedDesktopEntryData parseShortcut(const QString &id, const QString &path)
{
    ParsedDesktopEntryData data;
    data.id = id;

    auto name = QFileInfo(path).completeBaseName();
    data.name = name;
    data.genericName = name;

    auto coInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    auto guard = QScopeGuard([coInitialized] {
        if (coInitialized)
            CoUninitialize();
    });

    IShellLinkW *shellLink = nullptr;
    if (FAILED(CoCreateInstance(
            *resolveIid(CLSID_ShellLink),
            nullptr,
            CLSCTX_INPROC_SERVER,
            *resolveIid(IID_IShellLinkW),
            reinterpret_cast<void **>(&shellLink)
        )))
        return data;

    auto persist = [&]() -> IPersistFile * {
        IPersistFile *persistFile = nullptr;
        if (SUCCEEDED(shellLink->QueryInterface(
                *resolveIid(IID_IPersistFile), reinterpret_cast<void **>(&persistFile)
            )))
            return persistFile;
        return nullptr;
    }();

    if (!persist) {
        shellLink->Release();
        return data;
    }

    auto cleanup = QScopeGuard([&] {
        persist->Release();
        shellLink->Release();
    });

    if (FAILED(persist->Load(reinterpret_cast<LPCOLESTR>(path.utf16()), STGM_READ)))
        return data;

    wchar_t buffer[MAX_PATH];
    QString target;
    if (SUCCEEDED(shellLink->GetPath(buffer, MAX_PATH, nullptr, SLGP_RAWPATH))) {
        target = expandEnvironment(QString::fromWCharArray(buffer));
        data.execString = target;

        // StartupWMClass maps closest to the target's file name on Windows.
        data.startupClass = QFileInfo(target).completeBaseName();
    }

    if (SUCCEEDED(shellLink->GetWorkingDirectory(buffer, MAX_PATH)))
        data.workingDirectory = QString::fromWCharArray(buffer);

    QString arguments;
    if (SUCCEEDED(shellLink->GetArguments(buffer, MAX_PATH)) && buffer[0] != u'\0')
        arguments = expandEnvironment(QString::fromWCharArray(buffer)).trimmed();

    // A shortcut target is stored unquoted, so quote it before combining it with the
    // arguments. Without this, a path containing spaces is indistinguishable from
    // a program plus two arguments.
    if (!target.isEmpty()) {
        if (target.contains(u' ')) {
            data.execString = u'"' + target + u'"';
        }

        if (!arguments.isEmpty()) {
            data.execString += u' ';
            data.execString += arguments;
        }
    } else {
        data.execString = arguments;
    }

    // A shortcut is already a Windows command line, so it must be split with the platform's
    // quoting rules. The freedesktop `Exec` parser treats backslashes as escapes, which
    // would mangle every path.
    data.command = QProcess::splitCommand(data.execString);

    int iconIndex = 0;
    if (SUCCEEDED(shellLink->GetIconLocation(buffer, MAX_PATH, &iconIndex))
        && buffer[0] != u'\0') {
        data.icon = QString::fromWCharArray(buffer);
    }

    return data;
}
#endif

} // namespace

ParsedDesktopEntryData DesktopEntry::parseText(const QString &id, const QString &text)
{
    ParsedDesktopEntryData data;
    data.id = id;
    const auto &system = Locale::system();

    auto groupName = QString();
    auto entries = QHash<QString, QPair<Locale, QString>>();

    auto actionOrder = QStringList();
    auto pendingActions = QHash<QString, DesktopActionData>();

    auto finishCategory = [&data, &groupName, &entries, &actionOrder, &pendingActions]() {
        if (groupName == "Desktop Entry") {
            if (entries.value("Type").second != "Application")
                return;

            for (const auto &[key, pair]: entries.asKeyValueRange()) {
                auto &[_, value] = pair;
                data.entries.insert(key, value);

                if (key == "Name")
                    data.name = value;
                else if (key == "GenericName")
                    data.genericName = value;
                else if (key == "StartupWMClass")
                    data.startupClass = value;
                else if (key == "NoDisplay")
                    data.noDisplay = value == "true";
                else if (key == "Hidden")
                    data.hidden = value == "true";
                else if (key == "Comment")
                    data.comment = value;
                else if (key == "Icon")
                    data.icon = value;
                else if (key == "Exec") {
                    data.execString = value;
                    data.command = DesktopEntry::parseExecString(value);
                } else if (key == "Path")
                    data.workingDirectory = value;
                else if (key == "Terminal")
                    data.terminal = value == "true";
                else if (key == "Categories")
                    data.categories = value.split(u';', Qt::SkipEmptyParts);
                else if (key == "Keywords")
                    data.keywords = value.split(u';', Qt::SkipEmptyParts);
                else if (key == "Actions")
                    actionOrder = value.split(u';', Qt::SkipEmptyParts);
            }
        } else if (groupName.startsWith("Desktop Action ")) {
            auto actionName = groupName.sliced(15);
            DesktopActionData action;
            action.id = actionName;

            for (const auto &[key, pair]: entries.asKeyValueRange()) {
                const auto &[_, value] = pair;
                action.entries.insert(key, value);

                if (key == "Name")
                    action.name = value;
                else if (key == "Icon")
                    action.icon = value;
                else if (key == "Exec") {
                    action.execString = value;
                    action.command = DesktopEntry::parseExecString(value);
                }
            }

            pendingActions.insert(actionName, action);
        }

        entries.clear();
    };

    for (auto &line: text.split(u'\n', Qt::SkipEmptyParts)) {
        if (line.startsWith(u'#'))
            continue;

        if (line.startsWith(u'[') && line.endsWith(u']')) {
            finishCategory();
            groupName = line.sliced(1, line.length() - 2);
            continue;
        }

        auto splitIdx = line.indexOf(u'=');
        if (splitIdx == -1) {
            qCWarning(logDesktopEntry) << "Encountered invalid line in desktop entry (no =)" << line;
            continue;
        }

        auto key = line.sliced(0, splitIdx);
        const auto &value = line.sliced(splitIdx + 1);

        auto localeIdx = key.indexOf('[');
        Locale locale;
        if (localeIdx != -1 && localeIdx != key.length() - 1) {
            locale = Locale(key.sliced(localeIdx + 1, key.length() - localeIdx - 2));
            key = key.sliced(0, localeIdx);
        }

        if (entries.contains(key)) {
            const auto &old = entries.value(key);

            auto oldScore = system.matchScore(old.first);
            auto newScore = system.matchScore(locale);

            if (newScore > oldScore || (oldScore == 0 && !locale.isValid())) {
                entries.insert(key, qMakePair(locale, value));
            }
        } else {
            entries.insert(key, qMakePair(locale, value));
        }
    }

    finishCategory();

    for (const auto &actionId: actionOrder) {
        if (pendingActions.contains(actionId)) {
            data.actions.append(pendingActions.value(actionId));
        }
    }

    return data;
}

void DesktopEntry::updateState(const ParsedDesktopEntryData &newState)
{
    Qt::beginPropertyUpdateGroup();
    this->bName = newState.name;
    this->bGenericName = newState.genericName;
    this->bStartupClass = newState.startupClass;
    this->bNoDisplay = newState.noDisplay;
    this->bComment = newState.comment;
    this->bIcon = newState.icon;
    this->bExecString = newState.execString;
    this->bCommand = newState.command;
    this->bWorkingDirectory = newState.workingDirectory;
    this->bRunInTerminal = newState.terminal;
    this->bCategories = newState.categories;
    this->bKeywords = newState.keywords;
    Qt::endPropertyUpdateGroup();

    this->state = newState;
    this->updateActions(newState.actions);
}

void DesktopEntry::updateActions(const QVector<DesktopActionData> &newActions)
{
    auto old = this->mActions;
    this->mActions.clear();

    for (const auto &d: newActions) {
        DesktopAction *act = nullptr;
        auto found = std::ranges::find(old, d.id, &DesktopAction::mId);
        if (found != old.end()) {
            act = *found;
            old.erase(found);
        } else {
            act = new DesktopAction(d.id, this);
        }

        Qt::beginPropertyUpdateGroup();
        act->bName = d.name;
        act->bIcon = d.icon;
        act->bExecString = d.execString;
        act->bCommand = d.command;
        Qt::endPropertyUpdateGroup();

        act->mEntries = d.entries;
        this->mActions.append(act);
    }

    for (auto *leftover: old) {
        leftover->deleteLater();
    }
}

void DesktopEntry::execute() const
{
    DesktopEntry::doExec(this->bCommand.value(), this->bWorkingDirectory.value());
}

bool DesktopEntry::isValid() const { return !this->bName.value().isEmpty(); }

QVector<DesktopAction *> DesktopEntry::actions() const { return this->mActions; }

QVector<QString> DesktopEntry::parseExecString(const QString &execString)
{
    QVector<QString> arguments;
    QString currentArgument;
    auto parsingString = false;
    auto escape = 0;
    auto percent = false;

    for (auto c: execString) {
        if (escape == 0 && c == u'\\') {
            escape = 1;
        } else if (parsingString) {
            if (c == '\\') {
                escape++;
                if (escape == 4) {
                    currentArgument += '\\';
                    escape = 0;
                }
            } else if (escape == 2) {
                currentArgument += c;
                escape = 0;
            } else if (escape != 0) {
                switch (c.unicode()) {
                case 's':
                    currentArgument += u' ';
                    break;
                case 'n':
                    currentArgument += u'\n';
                    break;
                case 't':
                    currentArgument += u'\t';
                    break;
                case 'r':
                    currentArgument += u'\r';
                    break;
                case '\\':
                    currentArgument += u'\\';
                    break;
                default:
                    qCWarning(logDesktopEntry).noquote()
                        << "Illegal escape sequence in desktop entry exec string:" << execString;
                    currentArgument += c;
                    break;
                }
                escape = 0;
            } else if (c == u'"' || c == u'\'') {
                parsingString = false;
            } else {
                currentArgument += c;
            }
        } else if (escape != 0) {
            currentArgument += c;
            escape = 0;
        } else if (percent) {
            if (c == '%') {
                currentArgument += '%';
            } // else discard

            percent = false;
        } else if (c == '%') {
            percent = true;
        } else if (c == u'"' || c == u'\'') {
            parsingString = true;
        } else if (c == u' ') {
            if (!currentArgument.isEmpty()) {
                arguments.push_back(currentArgument);
                currentArgument.clear();
            }
        } else {
            currentArgument += c;
        }
    }

    if (!currentArgument.isEmpty()) {
        arguments.push_back(currentArgument);
        currentArgument.clear();
    }

    return arguments;
}

void DesktopEntry::doExec(const QList<QString> &execString, const QString &workingDirectory)
{
    if (execString.isEmpty())
        return;

    auto *shell = wqs::Quickshell::instance();
    if (!shell)
        return;

    QVariantMap context;
    context.insert(QStringLiteral("command"), execString);
    context.insert(QStringLiteral("workingDirectory"), workingDirectory);
    shell->execDetached(context);
}

void DesktopAction::execute() const
{
    DesktopEntry::doExec(this->bCommand.value(), this->entry->bWorkingDirectory.value());
}

DesktopEntryScanner::DesktopEntryScanner(DesktopEntryManager *manager) : manager(manager)
{
    this->setAutoDelete(true);
}

void DesktopEntryScanner::run()
{
    const auto &desktopPaths = DesktopEntryManager::desktopPaths();
    auto scanResults = QList<ParsedDesktopEntryData>();

    for (const auto &path: desktopPaths | std::views::reverse) {
        auto file = QFileInfo(path);
        if (!file.isDir())
            continue;

        this->scanDirectory(QDir(path), QString(), scanResults);
    }

    QMetaObject::invokeMethod(
        this->manager,
        "onScanCompleted",
        Qt::QueuedConnection,
        Q_ARG(QList<ParsedDesktopEntryData>, scanResults)
    );
}

void DesktopEntryScanner::scanDirectory(
    const QDir &dir, const QString &idPrefix, QList<ParsedDesktopEntryData> &entries
)
{
    auto dirEntries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (auto &entry: dirEntries) {
        if (entry.isDir()) {
            auto subdirPrefix =
                idPrefix.isEmpty() ? entry.fileName() : idPrefix + '-' + entry.fileName();
            this->scanDirectory(QDir(entry.absoluteFilePath()), subdirPrefix, entries);
            continue;
        }

        if (!entry.isFile())
            continue;

        auto path = entry.filePath();
        auto basename = QFileInfo(entry.fileName()).completeBaseName();
        auto id = idPrefix.isEmpty() ? basename : idPrefix + '-' + basename;

#ifdef Q_OS_WIN
        if (path.endsWith(".lnk", Qt::CaseInsensitive)) {
            auto data = parseShortcut(id, path);
            if (!data.execString.isEmpty())
                entries.append(std::move(data));
            continue;
        }
#endif

        if (!path.endsWith(".desktop")) {
            qCDebug(logDesktopEntry) << "Skipping file" << path << "as it has no .desktop extension";
            continue;
        }

        auto file = QFile(path);
        if (!file.open(QFile::ReadOnly)) {
            qCDebug(logDesktopEntry) << "Could not open file" << path;
            continue;
        }

        auto content = QString::fromUtf8(file.readAll());
        entries.append(DesktopEntry::parseText(id, content));
    }
}

DesktopEntryManager::DesktopEntryManager() : monitor(new DesktopEntryMonitor(this))
{
    QObject::connect(
        this->monitor,
        &DesktopEntryMonitor::desktopEntriesChanged,
        this,
        &DesktopEntryManager::handleFileChanges
    );

    DesktopEntryScanner(this).run();
}

void DesktopEntryManager::scanDesktopEntries()
{
    qCDebug(logDesktopEntry) << "Starting desktop entry scan";

    if (this->scanInProgress) {
        qCDebug(logDesktopEntry) << "Scan already in progress, queuing another scan";
        this->scanQueued = true;
        return;
    }

    this->scanInProgress = true;
    this->scanQueued = false;
    auto *scanner = new DesktopEntryScanner(this);
    QThreadPool::globalInstance()->start(scanner);
}

DesktopEntryManager *DesktopEntryManager::instance()
{
    static auto *instance = new DesktopEntryManager(); // NOLINT
    return instance;
}

DesktopEntry *DesktopEntryManager::byId(const QString &id)
{
    if (auto *entry = this->desktopEntries.value(id)) {
        return entry;
    } else if (auto *entry = this->lowercaseDesktopEntries.value(id.toLower())) {
        return entry;
    } else {
        return nullptr;
    }
}

DesktopEntry *DesktopEntryManager::heuristicLookup(const QString &name)
{
    if (auto *entry = this->byId(name))
        return entry;

    auto list = this->desktopEntries.values();

    auto iter = std::ranges::find_if(list, [&](DesktopEntry *entry) {
        return name == entry->bStartupClass.value();
    });

    if (iter != list.end())
        return *iter;

    iter = std::ranges::find_if(list, [&](DesktopEntry *entry) {
        return name.toLower() == entry->bStartupClass.value().toLower();
    });

    if (iter != list.end())
        return *iter;
    return nullptr;
}

ObjectModel<DesktopEntry> *DesktopEntryManager::applications() { return &this->mApplications; }

void DesktopEntryManager::handleFileChanges()
{
    qCDebug(logDesktopEntry) << "Directory change detected, performing full rescan";
    this->scanDesktopEntries();
}

const QStringList &DesktopEntryManager::desktopPaths()
{
    static const auto paths = []() {
        auto dataPaths = QStringList();

#ifdef Q_OS_WIN
        // the Start Menu is the Windows stand-in for the XDG application directories
        for (const auto &var: {"APPDATA", "ProgramData"}) {
            auto base = QDir::fromNativeSeparators(qEnvironmentVariable(var));
            if (base.isEmpty())
                continue;

            auto programs = QDir(base).filePath(QStringLiteral("Microsoft/Windows/Start Menu/Programs"));
            if (QDir(programs).exists())
                dataPaths.append(programs);
        }

        // honor the freedesktop layout if the user has explicitly pointed at it
        for (const auto &var: {"XDG_DATA_HOME", "XDG_DATA_DIRS"}) {
            auto value = qEnvironmentVariable(var);
            if (value.isEmpty())
                continue;

            for (const auto &dir: value.split(u':', Qt::SkipEmptyParts)) {
                auto applications = QDir(dir).filePath(QStringLiteral("applications"));
                if (QDir(applications).exists())
                    dataPaths.append(applications);
            }
        }

        return dataPaths;
#else
        auto dataHome = qEnvironmentVariable("XDG_DATA_HOME");
        if (dataHome.isEmpty() && qEnvironmentVariableIsSet("HOME"))
            dataHome = qEnvironmentVariable("HOME") + "/.local/share";
        if (!dataHome.isEmpty())
            dataPaths.append(dataHome + "/applications");

        auto dataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
        if (dataDirs.isEmpty())
            dataDirs = "/usr/local/share:/usr/share";

        for (const auto &dir: dataDirs.split(':', Qt::SkipEmptyParts)) {
            dataPaths.append(dir + "/applications");
        }

        return dataPaths;
#endif
    }();

    return paths;
}

void DesktopEntryManager::onScanCompleted(const QList<ParsedDesktopEntryData> &scanResults)
{
    auto guard = QScopeGuard([this] {
        this->scanInProgress = false;
        if (this->scanQueued) {
            this->scanQueued = false;
            this->scanDesktopEntries();
        }
    });

    auto oldEntries = this->desktopEntries;
    auto newEntries = QHash<QString, DesktopEntry *>();
    auto newLowercaseEntries = QHash<QString, DesktopEntry *>();

    for (const auto &data: scanResults) {
        auto lowerId = data.id.toLower();

        if (data.hidden) {
            if (auto *victim = newEntries.take(data.id))
                victim->deleteLater();
            newLowercaseEntries.remove(lowerId);

            if (auto it = oldEntries.find(data.id); it != oldEntries.end()) {
                it.value()->deleteLater();
                oldEntries.erase(it);
            }

            qCDebug(logDesktopEntry) << "Masking hidden desktop entry" << data.id;
            continue;
        }

        DesktopEntry *dentry = nullptr;

        if (auto it = oldEntries.find(data.id); it != oldEntries.end()) {
            dentry = it.value();
            oldEntries.erase(it);
            dentry->updateState(data);
        } else {
            dentry = new DesktopEntry(data.id, this);
            dentry->updateState(data);
        }

        if (!dentry->isValid()) {
            qCDebug(logDesktopEntry) << "Skipping desktop entry" << data.id;
            if (!oldEntries.contains(data.id)) {
                dentry->deleteLater();
            }
            continue;
        }

        qCDebug(logDesktopEntry) << "Found desktop entry" << data.id;

        if (newEntries.contains(data.id)) {
            qCDebug(logDesktopEntry) << "Replacing old entry for" << data.id;
            if (auto *victim = newEntries.take(data.id))
                victim->deleteLater();
            newLowercaseEntries.remove(lowerId);
        }

        newEntries.insert(data.id, dentry);

        if (newLowercaseEntries.contains(lowerId)) {
            qCInfo(logDesktopEntry).nospace()
                << "Multiple desktop entries have the same lowercased id " << lowerId
                << ". This can cause ambiguity when byId requests are not made with the correct "
                   "case already.";

            newLowercaseEntries.remove(lowerId);
        }

        newLowercaseEntries.insert(lowerId, dentry);
    }

    this->desktopEntries = newEntries;
    this->lowercaseDesktopEntries = newLowercaseEntries;

    auto newApplications = QVector<DesktopEntry *>();
    for (auto *entry: this->desktopEntries.values())
        if (!entry->bNoDisplay)
            newApplications.append(entry);

    this->mApplications.diffUpdate(newApplications);

    emit this->applicationsChanged();

    for (auto *e: oldEntries)
        e->deleteLater();
}

DesktopEntries::DesktopEntries()
{
    QObject::connect(
        DesktopEntryManager::instance(),
        &DesktopEntryManager::applicationsChanged,
        this,
        &DesktopEntries::applicationsChanged
    );
}

DesktopEntry *DesktopEntries::byId(const QString &id)
{
    return DesktopEntryManager::instance()->byId(id);
}

DesktopEntry *DesktopEntries::heuristicLookup(const QString &name)
{
    return DesktopEntryManager::instance()->heuristicLookup(name);
}

ObjectModel<DesktopEntry> *DesktopEntries::applications()
{
    return DesktopEntryManager::instance()->applications();
}