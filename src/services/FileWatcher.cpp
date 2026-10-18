#include "FileWatcher.h"
#include "core/ImageFormats.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace {
bool pathIsSameOrUnder(const QString &path, const QString &root)
{
    QString normalizedRoot = QDir::fromNativeSeparators(root);
    while (normalizedRoot.size() > 1 && normalizedRoot.endsWith('/'))
        normalizedRoot.chop(1);
    const QString normalizedPath = QDir::fromNativeSeparators(path);
    if (normalizedRoot == QLatin1String("/"))
        return normalizedPath.startsWith('/');
    return normalizedPath.compare(normalizedRoot, Qt::CaseInsensitive) == 0
        || normalizedPath.startsWith(normalizedRoot + '/', Qt::CaseInsensitive);
}
}

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

FileDiff FileWatcher::computeDiff(const QStringList &known, const QStringList &current)
{
    FileDiff diff;
    QSet<QString> knownSet;
    knownSet.reserve(known.size());
    for (const auto &k : known)
        knownSet.insert(k);
    QSet<QString> currentSet;
    currentSet.reserve(current.size());
    for (const auto &c : current)
        currentSet.insert(c);

    QSet<QString> addedSet;
    addedSet.reserve(qMax(known.size(), current.size()));
    for (const auto &c : current) {
        if (!knownSet.contains(c) && !addedSet.contains(c)) {
            addedSet.insert(c);
            diff.added.append(c);
        }
    }
    QSet<QString> removedSet;
    removedSet.reserve(qMax(known.size(), current.size()));
    for (const auto &k : known) {
        if (!currentSet.contains(k) && !removedSet.contains(k)) {
            removedSet.insert(k);
            diff.removed.append(k);
        }
    }
    return diff;
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
        if (pathIsSameOrUnder(dir, rootPath))
            toRemove.append(dir);
    }
    for (const auto &dir : toRemove)
        m_watcher->removePath(dir);

    auto it = m_knownByDir.begin();
    while (it != m_knownByDir.end()) {
        if (pathIsSameOrUnder(it.key(), rootPath))
            it = m_knownByDir.erase(it);
        else
            ++it;
    }
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
            // The directory may already contain files copied in before the
            // watcher was attached; schedule a scan so they are not missed.
            if (!m_pendingDirs.contains(subDir))
                m_pendingDirs.append(subDir);
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
    QStringList current;
    for (const auto &fi : entries) {
        if (!isSupportedImageFormat(fi.suffix())) continue;
        current.append(fi.absoluteFilePath());
    }

    const QStringList known = m_knownByDir.value(dirPath);
    FileDiff diff = computeDiff(known, current);

    for (const auto &path : diff.added)
        emit fileAdded(path);
    for (const auto &path : diff.removed)
        emit fileRemoved(path);

    m_knownByDir[dirPath] = current;
}
