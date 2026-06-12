#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QObject>
#include <QStringList>
#include "database/DatabaseManager.h"
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class LibraryController : public QObject
{
    Q_OBJECT
public:
    explicit LibraryController(QObject *parent = nullptr);
    ~LibraryController() override;

    bool initialize(const QString &dbPath);

    // Database operations
    DatabaseManager* db() const { return m_db; }

    // Asset queries
    QVector<Asset> loadAssets(const QString &keyword = {}, const QString &source = {},
                              const QVector<int> &tagIds = {}, bool onlyFavorites = false,
                              const QString &sortField = "created_at", bool sortAscending = false);
    Asset getAsset(const QString &id);
    Metadata getMetadata(const QString &assetId);
    QVector<Tag> getTagsForAsset(const QString &assetId);

    // Tag operations
    QVector<Tag> getAllTags();
    int createTag(const QString &name, const QColor &color = QColor(0x60, 0xa0, 0xff));
    bool renameTag(int tagId, const QString &newName);
    bool deleteTag(int tagId);
    void addTagToAssets(const QVector<QString> &assetIds, int tagId);
    void removeTagFromAsset(const QString &assetId, int tagId);

    // Scan & watch
    void addFolder(const QString &dir);
    void scanFolder(const QString &dir);
    void removeFolder(const QString &dir);
    QStringList folders() const { return m_folders; }
    void setFolders(const QStringList &folders) { m_folders = folders; }
    bool hasFolder(const QString &dir) const { return m_folders.contains(dir); }

    // File operations
    void scanAndInsertFile(const QString &path);
    bool deleteAssets(const QVector<Asset> &assets);
    void toggleFavorite(const QString &assetId, bool isFavorite);

    // Cache
    ImageCache* cache() const { return m_cache; }

signals:
    void assetCountChanged(int count);
    void scanProgress(int current, int total);
    void scanFinished();
    void dataChanged();

private:
    DatabaseManager *m_db;
    ImageCache *m_cache;
    FileScanner *m_scanner;
    FileWatcher *m_watcher;
    QStringList m_folders;
};

#endif
