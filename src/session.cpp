#include "session.h"
#include <QSettings>
#include <QStandardPaths>

// Keys used in the INI file.
namespace {
constexpr auto kWindowGeometry      = "window/geometry";
constexpr auto kVerticalSplitter    = "layout/vertical_splitter";
constexpr auto kTermSplitCount      = "terminal/split_count";
constexpr auto kTermSplitterState   = "terminal/splitter_state";
constexpr auto kTermExpanded        = "terminal/expanded";
constexpr auto kSideTermVisible     = "side_terminal/visible";
constexpr auto kSideTermSplitWidth  = "side_terminal/split_width";
constexpr auto kRecentFiles         = "recent/files";
constexpr auto kRecentFolders       = "recent/folders";

// Move (or insert) path to the front of the list and trim to maxEntries.
QStringList mruInsert(QStringList list, const QString &path, int maxEntries)
{
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > qMax(0, maxEntries))
        list.removeLast();
    return list;
}
} // namespace

QString Session::filePath()
{
    // Same directory as neobeam.toml (Config::configPath()), but a distinct file.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + "/session.ini";
}

Session::Session()
    : m_settings(new QSettings(filePath(), QSettings::IniFormat))
{
}

Session::~Session()
{
    sync();
    delete m_settings;
}

QByteArray Session::windowGeometry() const
{
    return m_settings->value(kWindowGeometry).toByteArray();
}

void Session::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(kWindowGeometry, geometry);
}

QByteArray Session::verticalSplitterState() const
{
    return m_settings->value(kVerticalSplitter).toByteArray();
}

void Session::setVerticalSplitterState(const QByteArray &state)
{
    m_settings->setValue(kVerticalSplitter, state);
}

bool Session::hasTerminalState() const
{
    return m_settings->contains(kTermSplitCount);
}

int Session::terminalSplitCount() const
{
    // At least one terminal must always exist.
    return qMax(1, m_settings->value(kTermSplitCount, 1).toInt());
}

void Session::setTerminalSplitCount(int count)
{
    m_settings->setValue(kTermSplitCount, qMax(1, count));
}

QByteArray Session::terminalSplitterState() const
{
    return m_settings->value(kTermSplitterState).toByteArray();
}

void Session::setTerminalSplitterState(const QByteArray &state)
{
    m_settings->setValue(kTermSplitterState, state);
}

bool Session::terminalExpanded() const
{
    return m_settings->value(kTermExpanded, false).toBool();
}

void Session::setTerminalExpanded(bool expanded)
{
    m_settings->setValue(kTermExpanded, expanded);
}

bool Session::sideTerminalVisible() const
{
    return m_settings->value(kSideTermVisible, false).toBool();
}

void Session::setSideTerminalVisible(bool visible)
{
    m_settings->setValue(kSideTermVisible, visible);
}

int Session::sideTerminalSplitWidth() const
{
    return qMax(0, m_settings->value(kSideTermSplitWidth, 0).toInt());
}

void Session::setSideTerminalSplitWidth(int width)
{
    m_settings->setValue(kSideTermSplitWidth, qMax(0, width));
}

QStringList Session::recentFiles() const
{
    return m_settings->value(kRecentFiles).toStringList();
}

void Session::addRecentFile(const QString &path, int maxEntries)
{
    if (path.isEmpty())
        return;
    m_settings->setValue(kRecentFiles, mruInsert(recentFiles(), path, maxEntries));
}

QStringList Session::recentFolders() const
{
    return m_settings->value(kRecentFolders).toStringList();
}

void Session::addRecentFolder(const QString &path, int maxEntries)
{
    if (path.isEmpty())
        return;
    m_settings->setValue(kRecentFolders, mruInsert(recentFolders(), path, maxEntries));
}

void Session::sync()
{
    m_settings->sync();
}
