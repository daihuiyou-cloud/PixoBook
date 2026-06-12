#include "FileWatcher.h"
#include "core/ImageFormats.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatcher::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileWatcher::onFileChanged);
}

void FileWatcher::addWatchPath(const QString &path)
{
    if (!QFileInfo::exists(path)) return;

    m_watcher->addPath(path);

    QDir dir(path);
    auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &fi : entries) {
        if (isSupportedImageFormat(fi.suffix()))
            m_knownFiles.append(fi.absoluteFilePath());
    }
}

void FileWatcher::removeWatchPath(const QString &path)
{
    m_watcher->removePath(path);
}

QStringList FileWatcher::watchedPaths() const
{
    return m_watcher->directories();
}

void FileWatcher::onDirectoryChanged(const QString &path)
{
    checkForNewFiles(path);
}

void FileWatcher::onFileChanged(const QString &path)
{
    if (!QFileInfo::exists(path))
        emit fileRemoved(path);
    else
        emit fileModified(path);
}

void FileWatcher::checkForNewFiles(const QString &dirPath)
{
    QDir dir(dirPath);
    auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    QSet<QString> current;

    for (const auto &fi : entries) {
        if (!isSupportedImageFormat(fi.suffix())) continue;
        current.insert(fi.absoluteFilePath());
        if (!m_knownFiles.contains(fi.absoluteFilePath()))
            emit fileAdded(fi.absoluteFilePath());
    }

    for (const auto &known : m_knownFiles) {
        if (!current.contains(known))
            emit fileRemoved(known);
    }

    m_knownFiles = current.values();
}
