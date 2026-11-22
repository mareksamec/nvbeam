#include "pathinput.h"

#include <QDir>
#include <QFileInfo>

QString collapseHome(const QString &path)
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

QString expandHome(const QString &path)
{
    if (path == QStringLiteral("~"))
        return QDir::homePath();
    if (path.startsWith(QStringLiteral("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

QString restartPath(const QString &text)
{
    // Walk back from the end to the last separator that is followed by the
    // start of a new path: a second separator ("//etc") or a home ("/~/x",
    // or "/~" as the last thing typed). The new path starts right after it.
    for (int i = text.size() - 2; i >= 0; --i) {
        if (text.at(i) != QLatin1Char('/'))
            continue;
        const QStringView rest = QStringView(text).mid(i + 1);
        if (rest.startsWith(QLatin1Char('/')) || rest.startsWith(QLatin1String("~/"))
            || rest == QLatin1String("~"))
            return rest.toString();
    }
    return text;
}

QString expandPathInput(const QString &text)
{
    return expandHome(restartPath(text.trimmed()));
}

QString resolvePath(const QString &text, const QString &base)
{
    const QString path = expandPathInput(text);
    if (path.isEmpty())
        return base;
    return QDir::isAbsolutePath(path) ? path : QDir(base).filePath(path);
}

CreateFolderResult createFolder(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir())
        return CreateFolderResult::AlreadyExists;
    if (info.exists())
        return CreateFolderResult::NotAFolder;
    // mkpath also fails when a parent component is a file, which lands here
    // as Failed: the message for that is the same "could not create".
    return QDir().mkpath(path) ? CreateFolderResult::Created : CreateFolderResult::Failed;
}
