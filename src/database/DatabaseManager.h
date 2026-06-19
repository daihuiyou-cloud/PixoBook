#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QVector>
#include "core/IDatabaseManager.h"
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class DatabaseManager : public IDatabaseManager
{
public:
    explicit DatabaseManager(const QString &dbPath);
    ~DatabaseManager() override;

    [[nodiscard]] bool initialize() override;

    [[nodiscard]] bool insertAsset(const Asset &asset) override;
    [[nodiscard]] bool updateAsset(const Asset &asset) override;
    [[nodiscard]] bool updateAssetFavorite(const QString &assetId, bool isFavorite) override;
    [[nodiscard]] bool deleteAsset(const QString &assetId) override;
    Asset getAsset(const QString &assetId) const override;
    QVector<Asset> getAllAssets() const override;
    QVector<Asset> searchAssets(const QString &keyword, const QString &source,
                                const QVector<int> &tagIds, bool onlyFavorites,
                                const QString &sortField = "created_at",
                                bool sortAscending = false,
                                int offset = 0, int limit = -1,
                                int *outTotalCount = nullptr) const override;
    Asset findByPath(const QString &filePath) const override;
    Asset findByHash(const QString &hash) const override;

    [[nodiscard]] bool upsertMetadata(const Metadata &metadata) override;
    Metadata getMetadata(const QString &assetId) const override;

    [[nodiscard]] int insertTag(const Tag &tag) override;
    [[nodiscard]] bool updateTag(const Tag &tag) override;
    [[nodiscard]] bool deleteTag(int tagId) override;
    Tag getTag(int tagId) const override;
    QVector<Tag> getAllTags() const override;

    [[nodiscard]] bool addTagToAsset(const QString &assetId, int tagId) override;
    [[nodiscard]] bool removeTagFromAsset(const QString &assetId, int tagId) override;
    QVector<int> getTagIdsForAsset(const QString &assetId) const override;
    QVector<Tag> getTagsForAsset(const QString &assetId) const override;
    QVector<Asset> getAssetsByTag(int tagId) const override;

    [[nodiscard]] bool beginTransaction() override;
    [[nodiscard]] bool commitTransaction() override;
    [[nodiscard]] bool rollbackTransaction() override;

    [[nodiscard]] int countAssets(const QString &keyword = {}, const QString &source = {},
                                    const QVector<int> &tagIds = {}, bool onlyFavorites = false) const override;

private:
    void createSchema();
    void migrate();
    QSqlDatabase m_db;
    QString m_connectionName;
};

#endif
