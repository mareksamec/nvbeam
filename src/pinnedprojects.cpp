#include "pinnedprojects.h"
#include "logging.h"

#include <QDir>
#include <algorithm>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <toml.hpp>

namespace {

/// One [[projects]] table. Written through toml11 rather than by hand so a
/// path or a command holding quotes or backslashes is escaped correctly.
toml::value tableFor(const ProjectEntry &project)
{
    toml::value table(toml::table{});
    table["path"] = project.path.toStdString();
    if (!project.name.isEmpty())
        table["name"] = project.name.toStdString();
    // One key per command, in the same order and under the same names the
    // main config uses (side_command, side_command_2, side_command_3).
    const QStringList &keys = sideCommandKeys();
    for (int i = 0; i < project.sideCommands.size() && i < keys.size(); ++i)
        table[keys.at(i).toStdString()] = project.sideCommands.at(i).toStdString();
    return table;
}

/// Read one string key, or an empty string when it is missing or not a string.
QString stringAt(const toml::value &table, const char *key)
{
    if (!table.contains(key) || !table.at(key).is_string())
        return QString();
    return QString::fromStdString(table.at(key).as_string());
}

/// Replace the file with these projects. Written through QSaveFile, so an
/// interrupted write leaves the previous list intact rather than a half file.
bool save(const QList<ProjectEntry> &projects)
{
    const QString path = PinnedProjects::filePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        qCWarning(nbSession) << "pinned projects: cannot create" << QFileInfo(path).absolutePath();
        return false;
    }

    toml::array entries;
    for (const ProjectEntry &project : projects)
        entries.push_back(tableFor(project));
    toml::value document(toml::table{{"projects", entries}});

    QByteArray out =
        "# Projects pinned from the neobeam switcher (Ctrl+P). Written by the\n"
        "# application: hand edits survive, but comments and layout do not.\n"
        "# Copy an entry into neobeam.toml to make it a permanent project.\n\n";
    out += QByteArray::fromStdString(toml::format(document));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(nbSession) << "pinned projects: cannot write" << path << file.errorString();
        return false;
    }
    file.write(out);
    if (!file.commit()) {
        qCWarning(nbSession) << "pinned projects: cannot commit" << path << file.errorString();
        return false;
    }
    return true;
}

} // namespace

QString PinnedProjects::filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/projects.toml");
}

QList<ProjectEntry> PinnedProjects::load()
{
    QList<ProjectEntry> projects;
    const QString path = filePath();
    if (!QFileInfo::exists(path))
        return projects;

    toml::value data;
    try {
        data = toml::parse(path.toStdString());
    } catch (const std::exception &e) {
        qCWarning(nbSession) << "pinned projects: cannot parse" << path << e.what();
        return projects;
    }

    if (!data.contains("projects") || !data.at("projects").is_array())
        return projects;

    for (const toml::value &entry : data.at("projects").as_array()) {
        if (!entry.is_table())
            continue;
        ProjectEntry project;
        project.path = stringAt(entry, "path");
        if (project.path.isEmpty())
            continue;
        project.path = QDir::cleanPath(project.path);
        project.name = stringAt(entry, "name");
        if (project.name.isEmpty())
            project.name = QFileInfo(project.path).fileName();
        for (const QString &key : sideCommandKeys()) {
            const QString command = stringAt(entry, key.toUtf8().constData());
            if (!command.isEmpty())
                project.sideCommands.append(command);
        }
        projects.append(project);
    }
    return projects;
}

bool PinnedProjects::add(const ProjectEntry &project)
{
    if (project.path.isEmpty())
        return false;
    QList<ProjectEntry> projects = load();
    for (const ProjectEntry &existing : projects) {
        if (existing.path == project.path)
            return false;
    }
    projects.append(project);
    return save(projects);
}

bool PinnedProjects::remove(const QString &path)
{
    QList<ProjectEntry> projects = load();
    const auto it = std::remove_if(projects.begin(), projects.end(),
                                   [&path](const ProjectEntry &p) { return p.path == path; });
    if (it == projects.end())
        return false;
    projects.erase(it, projects.end());
    return save(projects);
}
