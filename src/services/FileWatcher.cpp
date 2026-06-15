#include "FileWatcher.h"
#include "core/ImageFormats.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatcher::onDirectoryChanged);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        auto dirs = m_pendingDirs;
        m_pendingDirs.clear();
        QSet<QString> processed;
        for (const auto &d : dirs) {
            if (!processed.contains(d)) {
                processed.insert(d);
                checkForNewFiles(d);
            }
        }
    });
}

void FileWatcher::addWatchPath(const QString &path)
{
    if (!QFileInfo::exists(path)) return;
    watchDirectoryTree(path);
    checkForNewFiles(path);
}

void FileWatcher::watchDirectoryTree(const QString &rootPath)
{
    if (m_watchedDirs.contains(rootPath)) return;
    m_watchedDirs.insert(rootPath);

    QDirIterator it(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    m_watcher->addPath(rootPath);
    while (it.hasNext()) {
        it.next();
        m_watcher->addPath(it.filePath());
    }
}

void FileWatcher::unwatchDirectoryTree(const QString &rootPath)
{
    m_watchedDirs.remove(rootPath);
    QStringList toRemove;
    for (const auto &dir : m_watcher->directories()) {
        if (dir.startsWith(rootPath))
            toRemove.append(dir);
    }
    for (const auto &dir : toRemove)
        m_watcher->removePath(dir);

    m_knownFiles.erase(
        std::remove_if(m_knownFiles.begin(), m_knownFiles.end(),
                       [&](const QString &f) { return f.startsWith(rootPath); }),
        m_knownFiles.end());
}

void FileWatcher::removeWatchPath(const QString &path)
{
    unwatchDirectoryTree(path);
}

QStringList FileWatcher::watchedPaths() const
{
    return m_watchedDirs.values();
}

void FileWatcher::onDirectoryChanged(const QString &path)
{
    // Check for new subdirectories and watch them
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        QString subDir = it.filePath();
        if (!m_watcher->directories().contains(subDir)) {
            m_watcher->addPath(subDir);
        }
    }

    if (!m_pendingDirs.contains(path))
        m_pendingDirs.append(path);
    m_debounceTimer.start();
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
