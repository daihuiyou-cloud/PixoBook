#ifndef IDATABASEMANAGER_H
#define IDATABASEMANAGER_H

#include <QString>
#include <QVector>
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class IDatabaseManager
{
public:
    virtual ~IDatabaseManager() = default;

    virtual bool initialize() = 0;

    // Asset CRUD
    virtual bool insertAsset(const Asset &asset) = 0;
    virtual bool updateAsset(const Asset &asset) = 0;
    virtual bool updateAssetFavorite(const QString &assetId, bool isFavorite) = 0;
    virtual bool deleteAsset(const QString &assetId) = 0;
    virtual Asset getAsset(const QString &assetId) const = 0;
    virtual QVector<Asset> getAllAssets() const = 0;
    virtual QVector<Asset> searchAssets(const QString &keyword, const QString &source,
                                        const QVector<int> &tagIds, bool onlyFavorites,
                                        const QString &sortField = "created_at",
                                        bool sortAscending = false,
                                        int offset = 0, int limit = -1) const = 0;
    virtual Asset findByPath(const QString &filePath) const = 0;
    virtual Asset findByHash(const QString &hash) const = 0;

    // Metadata
    virtual bool upsertMetadata(const Metadata &metadata) = 0;
    virtual Metadata getMetadata(const QString &assetId) const = 0;

    // Tags
    virtual int insertTag(const Tag &tag) = 0;
    virtual bool updateTag(const Tag &tag) = 0;
    virtual bool deleteTag(int tagId) = 0;
    virtual QVector<Tag> getAllTags() const = 0;

    // Asset-Tag relations
    virtual bool addTagToAsset(const QString &assetId, int tagId) = 0;
    virtual bool removeTagFromAsset(const QString &assetId, int tagId) = 0;
    virtual QVector<int> getTagIdsForAsset(const QString &assetId) const = 0;
    virtual QVector<Tag> getTagsForAsset(const QString &assetId) const = 0;
    virtual QVector<Asset> getAssetsByTag(int tagId) const = 0;

    // Transaction support
    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;

    // Count queries
    virtual int countAssets(const QString &keyword = {}, const QString &source = {},
                            const QVector<int> &tagIds = {}, bool onlyFavorites = false) const = 0;
};

#endif
