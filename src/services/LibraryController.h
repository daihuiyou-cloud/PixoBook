#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QObject>
#include <QStringList>
#include "core/IDatabaseManager.h"
#include "core/IImageCache.h"
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class FileScanner;
class FileWatcher;

class LibraryController : public QObject
{
    Q_OBJECT
public:
    explicit LibraryController(IDatabaseManager *db, IImageCache *cache,
                               FileScanner *scanner, FileWatcher *watcher,
                               QObject *parent = nullptr);
    ~LibraryController() override;

    IImageCache* cache() const { return m_cache; }
    IDatabaseManager* db() const { return m_db; }

    // Asset queries
    QVector<Asset> loadAssets(const QString &keyword = {}, const QString &source = {},
                              const QVector<int> &tagIds = {}, bool onlyFavorites = false,
                              const QString &sortField = "created_at", bool sortAscending = false,
                              int offset = 0, int limit = -1) const;
    Asset getAsset(const QString &id) const;
    Metadata getMetadata(const QString &assetId) const;
    QVector<Tag> getTagsForAsset(const QString &assetId) const;
    [[nodiscard]] bool updateMetadata(const Metadata &metadata);

    // Tag operations
    QVector<Tag> getAllTags() const;
    [[nodiscard]] int createTag(const QString &name, const QColor &color = QColor(0x60, 0xa0, 0xff));
    [[nodiscard]] bool renameTag(int tagId, const QString &newName);
    [[nodiscard]] bool deleteTag(int tagId);
    void addTagToAssets(const QVector<QString> &assetIds, int tagId);
    void removeTagFromAsset(const QString &assetId, int tagId);

    // Scan & watch
    void addFolder(const QString &dir);
    void scanFolder(const QString &dir);
    void removeFolder(const QString &dir);
    QStringList folders() const { return m_folders; }
    void setFolders(const QStringList &folders) { m_folders = folders; }
    [[nodiscard]] bool hasFolder(const QString &dir) const { return m_folders.contains(dir); }

    // File operations
    void scanAndInsertFile(const QString &path);
    [[nodiscard]] bool deleteAssets(const QVector<Asset> &assets);
    void toggleFavorite(const QString &assetId, bool isFavorite);

    // Count
    int countAssets(const QString &keyword = {}, const QString &source = {},
                    const QVector<int> &tagIds = {}, bool onlyFavorites = false) const;

signals:
    void assetCountChanged(int count);
    void scanProgress(int current, int total);
    void scanFinished();
    void dataChanged();

private:
    IDatabaseManager *m_db;
    IImageCache *m_cache;
    FileScanner *m_scanner;
    FileWatcher *m_watcher;
    QStringList m_folders;
};

#endif
