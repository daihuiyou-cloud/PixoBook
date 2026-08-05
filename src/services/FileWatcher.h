#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QHash>

struct FileDiff {
    QStringList added;
    QStringList removed;
};

class FileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FileWatcher(QObject *parent = nullptr);

    void addWatchPath(const QString &path);
    void removeWatchPath(const QString &path);
    QStringList watchedPaths() const;

    static FileDiff computeDiff(const QStringList &known, const QStringList &current);

signals:
    void fileAdded(const QString &filePath);
    void fileRemoved(const QString &filePath);

private slots:
    void onDirectoryChanged(const QString &path);

private:
    void watchDirectoryTree(const QString &rootPath);
    void unwatchDirectoryTree(const QString &rootPath);
    void checkForNewFiles(const QString &dirPath);
    QFileSystemWatcher *m_watcher;
    QHash<QString, QStringList> m_knownByDir;
    QSet<QString> m_watchedDirs;
    QTimer m_debounceTimer;
    QStringList m_pendingDirs;
};

#endif
