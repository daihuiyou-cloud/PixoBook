#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>

class FileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FileWatcher(QObject *parent = nullptr);

    void addWatchPath(const QString &path);
    void removeWatchPath(const QString &path);
    QStringList watchedPaths() const;

signals:
    void fileAdded(const QString &filePath);
    void fileRemoved(const QString &filePath);
    void fileModified(const QString &filePath);

private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    QFileSystemWatcher *m_watcher;
    void checkForNewFiles(const QString &dirPath);
    QStringList m_knownFiles;
};

#endif
