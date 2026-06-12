#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QVector>
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class DatabaseManager
{
public:
    explicit DatabaseManager(const QString &dbPath);
    ~DatabaseManager();

    bool initialize();

    bool insertAsset(const Asset &asset);
    bool updateAsset(const Asset &asset);
    bool updateAssetFavorite(const QString &assetId, bool isFavorite);
    bool deleteAsset(const QString &assetId);
    Asset getAsset(const QString &assetId) const;
    QVector<Asset> getAllAssets() const;
    QVector<Asset> searchAssets(const QString &keyword, const QString &source,
                                const QVector<int> &tagIds, bool onlyFavorites,
                                const QString &sortField = "created_at",
                                bool sortAscending = false) const;
    Asset findByPath(const QString &filePath) const;
    Asset findByHash(const QString &hash) const;

    bool upsertMetadata(const Metadata &metadata);
    Metadata getMetadata(const QString &assetId) const;

    int insertTag(const Tag &tag);
    bool updateTag(const Tag &tag);
    bool deleteTag(int tagId);
    QVector<Tag> getAllTags() const;

    bool addTagToAsset(const QString &assetId, int tagId);
    bool removeTagFromAsset(const QString &assetId, int tagId);
    QVector<int> getTagIdsForAsset(const QString &assetId) const;
    QVector<Tag> getTagsForAsset(const QString &assetId) const;
    QVector<Asset> getAssetsByTag(int tagId) const;

private:
    void createSchema();
    void migrate();
    QSqlDatabase m_db;
};

#endif
