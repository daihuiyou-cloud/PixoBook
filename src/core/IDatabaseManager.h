#ifndef IDATABASEMANAGER_H
#define IDATABASEMANAGER_H

#include <QHash>
#include <QString>
#include <QVector>
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class IDatabaseManager
{
public:
    virtual ~IDatabaseManager() = default;

    [[nodiscard]] virtual bool initialize() = 0;

    // Asset CRUD
    [[nodiscard]] virtual bool insertAsset(const Asset &asset) = 0;
    [[nodiscard]] virtual bool updateAsset(const Asset &asset) = 0;
    [[nodiscard]] virtual bool updateAssetFavorite(const QString &assetId, bool isFavorite) = 0;
    [[nodiscard]] virtual bool deleteAsset(const QString &assetId) = 0;
    virtual Asset getAsset(const QString &assetId) const = 0;
    virtual QVector<Asset> getAllAssets() const = 0;
    virtual QVector<Asset> searchAssets(const QString &keyword, const QString &source,
                                        const QVector<int> &tagIds, bool onlyFavorites,
                                        const QString &sortField = "created_at",
                                        bool sortAscending = false,
                                        int offset = 0, int limit = -1,
                                        int *outTotalCount = nullptr,
                                        const QString &folderPath = QString()) const = 0;
    virtual Asset findByPath(const QString &filePath) const = 0;
    virtual Asset findByHash(const QString &hash) const = 0;

    // File presence. A watched path that disappears is marked missing instead
    // of being deleted, so tags/favorites survive a moved drive, a stale
    // network share, or a temporarily missing folder.
    [[nodiscard]] virtual bool markAssetMissing(const QString &filePath,
                                                const QDateTime &when) = 0;
    [[nodiscard]] virtual bool markAssetPresent(const QString &assetId) = 0;
    [[nodiscard]] virtual int countMissingAssets() const = 0;
    virtual QVector<Asset> getMissingAssets() const = 0;
    [[nodiscard]] virtual bool deleteMissingAssets() = 0;

    // Annotation export: only rows that carry user data (tag, favorite or
    // parsed metadata) plus batch lookups so export stays a fixed number of
    // queries instead of one per asset.
    virtual QVector<Asset> getAnnotatedAssets() const = 0;
    virtual QHash<QString, Metadata> getAllMetadata() const = 0;
    virtual QHash<QString, QVector<Tag>> getAllAssetTags() const = 0;

    // Metadata
    [[nodiscard]] virtual bool upsertMetadata(const Metadata &metadata) = 0;
    virtual Metadata getMetadata(const QString &assetId) const = 0;

    // Tags
    [[nodiscard]] virtual int insertTag(const Tag &tag) = 0;
    [[nodiscard]] virtual bool updateTag(const Tag &tag) = 0;
    [[nodiscard]] virtual bool deleteTag(int tagId) = 0;
    virtual Tag getTag(int tagId) const = 0;
    virtual QVector<Tag> getAllTags() const = 0;

    // Asset-Tag relations
    [[nodiscard]] virtual bool addTagToAsset(const QString &assetId, int tagId) = 0;
    [[nodiscard]] virtual bool removeTagFromAsset(const QString &assetId, int tagId) = 0;
    virtual QVector<int> getTagIdsForAsset(const QString &assetId) const = 0;
    virtual QVector<Tag> getTagsForAsset(const QString &assetId) const = 0;
    virtual QVector<Asset> getAssetsByTag(int tagId) const = 0;

    // During a folder scan the controller defers per-row indexing and asks
    // for one batch at the end; otherwise FTS sync is immediate.
    virtual void setDeferSearchIndexing(bool defer) = 0;
    [[nodiscard]] virtual bool indexPendingAssets() = 0;

    // Transaction support
    [[nodiscard]] virtual bool beginTransaction() = 0;
    [[nodiscard]] virtual bool commitTransaction() = 0;
    [[nodiscard]] virtual bool rollbackTransaction() = 0;

    // Count queries
    [[nodiscard]] virtual int countAssets(const QString &keyword = {}, const QString &source = {},
                                          const QVector<int> &tagIds = {}, bool onlyFavorites = false,
                                          const QString &folderPath = QString()) const = 0;
};

#endif
