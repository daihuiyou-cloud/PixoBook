#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

DatabaseManager::DatabaseManager(const QString &dbPath)
{
    m_connectionName = QStringLiteral("pixobook_%1").arg(dbPath);
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
    if (QSqlDatabase::connectionNames().contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

namespace {
QDateTime parseIsoDt(const QString &s, Qt::DateFormat fmt = Qt::ISODate)
{
    auto dt = QDateTime::fromString(s, fmt);
    return dt.isValid() ? dt : QDateTime();
}
}

bool DatabaseManager::initialize()
{
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    createSchema();
    migrate();
    return true;
}

void DatabaseManager::createSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec("PRAGMA journal_mode=WAL"))
        qWarning("Failed to enable WAL mode: %s", q.lastError().text().toUtf8().constData());
    if (!q.exec("PRAGMA foreign_keys=ON"))
        qWarning("Failed to enable foreign keys: %s", q.lastError().text().toUtf8().constData());

    q.exec(
        "CREATE TABLE IF NOT EXISTS assets ("
        "  id TEXT PRIMARY KEY,"
        "  file_path TEXT UNIQUE NOT NULL,"
        "  file_name TEXT NOT NULL,"
        "  file_size INTEGER NOT NULL DEFAULT 0,"
        "  width INTEGER NOT NULL DEFAULT 0,"
        "  height INTEGER NOT NULL DEFAULT 0,"
        "  format TEXT NOT NULL DEFAULT '',"
        "  hash TEXT NOT NULL DEFAULT '',"
        "  is_favorite INTEGER NOT NULL DEFAULT 0,"
        "  folder_id TEXT NOT NULL DEFAULT '',"
        "  created_at TEXT NOT NULL,"
        "  updated_at TEXT NOT NULL"
        ")"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS metadata ("
        "  asset_id TEXT PRIMARY KEY,"
        "  source TEXT NOT NULL DEFAULT '',"
        "  prompt TEXT NOT NULL DEFAULT '',"
        "  negative_prompt TEXT NOT NULL DEFAULT '',"
        "  seed INTEGER NOT NULL DEFAULT 0,"
        "  steps INTEGER NOT NULL DEFAULT 0,"
        "  cfg_scale REAL NOT NULL DEFAULT 0.0,"
        "  model_name TEXT NOT NULL DEFAULT '',"
        "  sampler TEXT NOT NULL DEFAULT '',"
        "  raw_json TEXT NOT NULL DEFAULT '',"
        "  FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE"
        ")"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS tags ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT UNIQUE NOT NULL,"
        "  color TEXT NOT NULL DEFAULT '#888888'"
        ")"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS asset_tags ("
        "  asset_id TEXT NOT NULL,"
        "  tag_id INTEGER NOT NULL,"
        "  PRIMARY KEY (asset_id, tag_id),"
        "  FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
        ")"
    );

    q.exec("CREATE INDEX IF NOT EXISTS idx_assets_hash ON assets(hash)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_assets_folder ON assets(folder_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_assets_favorite ON assets(is_favorite)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_assets_created ON assets(created_at)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_metadata_source ON metadata(source)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_asset_tags_tag ON asset_tags(tag_id)");

    q.exec(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER PRIMARY KEY"
        ")"
    );
}

void DatabaseManager::migrate()
{
    QSqlQuery q(m_db);
    q.exec("SELECT MAX(version) FROM schema_version");
    int version = 0;
    if (q.next())
        version = q.value(0).toInt();

    if (version < 1) {
        // v1: initial schema
        q.exec("INSERT OR REPLACE INTO schema_version (version) VALUES (1)");
        version = 1;
    }

    if (version < 2) {
        // v2: future migration placeholder
        // q.exec("ALTER TABLE assets ADD COLUMN ...");
        q.exec("INSERT OR REPLACE INTO schema_version (version) VALUES (2)");
        version = 2;
    }
}

bool DatabaseManager::insertAsset(const Asset &asset)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO assets (id, file_path, file_name, file_size, width, height, "
              "format, hash, is_favorite, folder_id, created_at, updated_at) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(asset.id);
    q.addBindValue(asset.filePath.isNull() ? "" : asset.filePath);
    q.addBindValue(asset.fileName.isNull() ? "" : asset.fileName);
    q.addBindValue(asset.fileSize);
    q.addBindValue(asset.width);
    q.addBindValue(asset.height);
    q.addBindValue(asset.format.isNull() ? "" : asset.format);
    q.addBindValue(asset.hash.isNull() ? "" : asset.hash);
    q.addBindValue(asset.isFavorite ? 1 : 0);
    q.addBindValue(asset.folderId.isNull() ? "" : asset.folderId);
    q.addBindValue(asset.createdAt.toString(Qt::ISODate));
    q.addBindValue(asset.updatedAt.toString(Qt::ISODate));
    if (!q.exec()) {
        qWarning() << "insertAsset failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateAsset(const Asset &asset)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assets SET file_path=?, file_name=?, file_size=?, width=?, height=?, "
              "format=?, hash=?, is_favorite=?, folder_id=?, updated_at=? WHERE id=?");
    q.addBindValue(asset.filePath);
    q.addBindValue(asset.fileName);
    q.addBindValue(asset.fileSize);
    q.addBindValue(asset.width);
    q.addBindValue(asset.height);
    q.addBindValue(asset.format);
    q.addBindValue(asset.hash);
    q.addBindValue(asset.isFavorite ? 1 : 0);
    q.addBindValue(asset.folderId);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(asset.id);
    return q.exec();
}

bool DatabaseManager::updateAssetFavorite(const QString &assetId, bool isFavorite)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assets SET is_favorite=?, updated_at=? WHERE id=?");
    q.addBindValue(isFavorite ? 1 : 0);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(assetId);
    return q.exec();
}

bool DatabaseManager::deleteAsset(const QString &assetId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM assets WHERE id=?");
    q.addBindValue(assetId);
    return q.exec();
}

Asset DatabaseManager::getAsset(const QString &assetId) const
{
    Asset a;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM assets WHERE id=?");
    q.addBindValue(assetId);
    if (q.exec() && q.next()) {
        a.id = q.value("id").toString();
        a.filePath = q.value("file_path").toString();
        a.fileName = q.value("file_name").toString();
        a.fileSize = q.value("file_size").toLongLong();
        a.width = q.value("width").toInt();
        a.height = q.value("height").toInt();
        a.format = q.value("format").toString();
        a.hash = q.value("hash").toString();
        a.isFavorite = q.value("is_favorite").toInt() != 0;
        a.folderId = q.value("folder_id").toString();
        a.createdAt = parseIsoDt(q.value("created_at").toString(), Qt::ISODate);
        a.updatedAt = parseIsoDt(q.value("updated_at").toString(), Qt::ISODate);
    }
    return a;
}

QVector<Asset> DatabaseManager::getAllAssets() const
{
    QVector<Asset> assets;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM assets ORDER BY created_at DESC")) {
        while (q.next()) {
            Asset a;
            a.id = q.value("id").toString();
            a.filePath = q.value("file_path").toString();
            a.fileName = q.value("file_name").toString();
            a.fileSize = q.value("file_size").toLongLong();
            a.width = q.value("width").toInt();
            a.height = q.value("height").toInt();
            a.format = q.value("format").toString();
            a.hash = q.value("hash").toString();
            a.isFavorite = q.value("is_favorite").toInt() != 0;
            a.folderId = q.value("folder_id").toString();
            a.createdAt = parseIsoDt(q.value("created_at").toString(), Qt::ISODate);
            a.updatedAt = parseIsoDt(q.value("updated_at").toString(), Qt::ISODate);
            assets.append(a);
        }
    }
    return assets;
}

QVector<Asset> DatabaseManager::searchAssets(const QString &keyword, const QString &source,
                                              const QVector<int> &tagIds, bool onlyFavorites,
                                              const QString &sortField, bool sortAscending,
                                              int offset, int limit) const
{
    QVector<Asset> assets;
    QString sql = "SELECT DISTINCT a.*, m.source AS metadata_source, m.prompt AS metadata_prompt FROM assets a "
                  "LEFT JOIN metadata m ON a.id = m.asset_id "
                  "LEFT JOIN asset_tags at ON a.id = at.asset_id "
                  "WHERE 1=1 ";
    QVector<QVariant> binds;

    if (!keyword.isEmpty()) {
        sql += "AND (a.file_name LIKE ? OR m.prompt LIKE ?) ";
        auto kw = "%" + keyword + "%";
        binds.append(kw);
        binds.append(kw);
    }
    if (!source.isEmpty()) {
        sql += "AND m.source = ? ";
        binds.append(source);
    }
    if (onlyFavorites) {
        sql += "AND a.is_favorite = 1 ";
    }
    if (!tagIds.isEmpty()) {
        QStringList placeholders;
        for (auto tid : tagIds) {
            placeholders << "?";
            binds.append(tid);
        }
        sql += "AND at.tag_id IN (" + placeholders.join(",") + ") ";
    }
    // Validate sort field to prevent SQL injection
    QString sortCol = "a.created_at";
    if (sortField == QLatin1String("file_name")) sortCol = "a.file_name";
    else if (sortField == QLatin1String("file_size")) sortCol = "a.file_size";
    sql += "ORDER BY " + sortCol + " " + (sortAscending ? "ASC" : "DESC");

    if (limit > 0) {
        sql += " LIMIT ? OFFSET ?";
        binds.append(limit);
        binds.append(offset);
    }

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const auto &b : binds)
        q.addBindValue(b);

    if (q.exec()) {
        while (q.next()) {
            Asset a;
            a.id = q.value("id").toString();
            a.filePath = q.value("file_path").toString();
            a.fileName = q.value("file_name").toString();
            a.fileSize = q.value("file_size").toLongLong();
            a.width = q.value("width").toInt();
            a.height = q.value("height").toInt();
            a.format = q.value("format").toString();
            a.hash = q.value("hash").toString();
            a.isFavorite = q.value("is_favorite").toInt() != 0;
            a.folderId = q.value("folder_id").toString();
            a.metadataSource = q.value("metadata_source").toString();
            a.metadataPrompt = q.value("metadata_prompt").toString();
            a.createdAt = parseIsoDt(q.value("created_at").toString(), Qt::ISODate);
            a.updatedAt = parseIsoDt(q.value("updated_at").toString(), Qt::ISODate);
            assets.append(a);
        }
    }
    return assets;
}

int DatabaseManager::countAssets(const QString &keyword, const QString &source,
                                  const QVector<int> &tagIds, bool onlyFavorites) const
{
    QString sql = "SELECT COUNT(DISTINCT a.id) FROM assets a "
                  "LEFT JOIN metadata m ON a.id = m.asset_id "
                  "LEFT JOIN asset_tags at ON a.id = at.asset_id "
                  "WHERE 1=1 ";
    QVector<QVariant> binds;

    if (!keyword.isEmpty()) {
        sql += "AND (a.file_name LIKE ? OR m.prompt LIKE ?) ";
        auto kw = "%" + keyword + "%";
        binds.append(kw);
        binds.append(kw);
    }
    if (!source.isEmpty()) {
        sql += "AND m.source = ? ";
        binds.append(source);
    }
    if (onlyFavorites) {
        sql += "AND a.is_favorite = 1 ";
    }
    if (!tagIds.isEmpty()) {
        QStringList placeholders;
        for (auto tid : tagIds) {
            placeholders << "?";
            binds.append(tid);
        }
        sql += "AND at.tag_id IN (" + placeholders.join(",") + ") ";
    }

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const auto &b : binds)
        q.addBindValue(b);

    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

Asset DatabaseManager::findByPath(const QString &filePath) const
{
    Asset a;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM assets WHERE file_path=?");
    q.addBindValue(filePath);
    if (q.exec() && q.next()) {
        a.id = q.value("id").toString();
        a.filePath = q.value("file_path").toString();
        a.fileName = q.value("file_name").toString();
        a.fileSize = q.value("file_size").toLongLong();
        a.width = q.value("width").toInt();
        a.height = q.value("height").toInt();
        a.format = q.value("format").toString();
        a.hash = q.value("hash").toString();
        a.isFavorite = q.value("is_favorite").toInt() != 0;
        a.folderId = q.value("folder_id").toString();
        a.createdAt = parseIsoDt(q.value("created_at").toString(), Qt::ISODate);
        a.updatedAt = parseIsoDt(q.value("updated_at").toString(), Qt::ISODate);
    }
    return a;
}

Asset DatabaseManager::findByHash(const QString &hash) const
{
    Asset a;
    if (hash.isEmpty()) return a;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM assets WHERE hash=?");
    q.addBindValue(hash);
    if (q.exec() && q.next()) {
        a.id = q.value("id").toString();
        a.filePath = q.value("file_path").toString();
        a.fileName = q.value("file_name").toString();
        a.fileSize = q.value("file_size").toLongLong();
        a.width = q.value("width").toInt();
        a.height = q.value("height").toInt();
        a.format = q.value("format").toString();
        a.hash = q.value("hash").toString();
        a.isFavorite = q.value("is_favorite").toInt() != 0;
        a.folderId = q.value("folder_id").toString();
        a.createdAt = parseIsoDt(q.value("created_at").toString(), Qt::ISODate);
        a.updatedAt = parseIsoDt(q.value("updated_at").toString(), Qt::ISODate);
    }
    return a;
}

bool DatabaseManager::upsertMetadata(const Metadata &metadata)
{
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT OR REPLACE INTO metadata "
        "(asset_id, source, prompt, negative_prompt, seed, steps, cfg_scale, model_name, sampler, raw_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(metadata.assetId);
    q.addBindValue(metadata.source);
    q.addBindValue(metadata.prompt);
    q.addBindValue(metadata.negativePrompt);
    q.addBindValue(metadata.seed);
    q.addBindValue(metadata.steps);
    q.addBindValue(metadata.cfgScale);
    q.addBindValue(metadata.modelName);
    q.addBindValue(metadata.sampler);
    q.addBindValue(metadata.rawJson);
    if (!q.exec()) {
        qWarning() << "upsertMetadata failed:" << q.lastError().text();
        return false;
    }
    return true;
}

Metadata DatabaseManager::getMetadata(const QString &assetId) const
{
    Metadata m;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM metadata WHERE asset_id=?");
    q.addBindValue(assetId);
    if (q.exec() && q.next()) {
        m.assetId = q.value("asset_id").toString();
        m.source = q.value("source").toString();
        m.prompt = q.value("prompt").toString();
        m.negativePrompt = q.value("negative_prompt").toString();
        m.seed = q.value("seed").toLongLong();
        m.steps = q.value("steps").toInt();
        m.cfgScale = q.value("cfg_scale").toDouble();
        m.modelName = q.value("model_name").toString();
        m.sampler = q.value("sampler").toString();
        m.rawJson = q.value("raw_json").toString();
    }
    return m;
}

int DatabaseManager::insertTag(const Tag &tag)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO tags (name, color) VALUES (?, ?)");
    q.addBindValue(tag.name);
    q.addBindValue(tag.color.name());
    if (!q.exec()) {
        qWarning() << "insertTag failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::updateTag(const Tag &tag)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE tags SET name=?, color=? WHERE id=?");
    q.addBindValue(tag.name);
    q.addBindValue(tag.color.name());
    q.addBindValue(tag.id);
    return q.exec();
}

bool DatabaseManager::deleteTag(int tagId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM tags WHERE id=?");
    q.addBindValue(tagId);
    return q.exec();
}

QVector<Tag> DatabaseManager::getAllTags() const
{
    QVector<Tag> tags;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM tags ORDER BY name")) {
        while (q.next()) {
            Tag t;
            t.id = q.value("id").toInt();
            t.name = q.value("name").toString();
            t.color = QColor(q.value("color").toString());
            tags.append(t);
        }
    }
    return tags;
}

bool DatabaseManager::addTagToAsset(const QString &assetId, int tagId)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO asset_tags (asset_id, tag_id) VALUES (?, ?)");
    q.addBindValue(assetId);
    q.addBindValue(tagId);
    return q.exec();
}

bool DatabaseManager::removeTagFromAsset(const QString &assetId, int tagId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM asset_tags WHERE asset_id=? AND tag_id=?");
    q.addBindValue(assetId);
    q.addBindValue(tagId);
    return q.exec();
}

QVector<int> DatabaseManager::getTagIdsForAsset(const QString &assetId) const
{
    QVector<int> ids;
    QSqlQuery q(m_db);
    q.prepare("SELECT tag_id FROM asset_tags WHERE asset_id=?");
    q.addBindValue(assetId);
    if (q.exec()) {
        while (q.next())
            ids.append(q.value(0).toInt());
    }
    return ids;
}

QVector<Tag> DatabaseManager::getTagsForAsset(const QString &assetId) const
{
    QVector<Tag> tags;
    QSqlQuery q(m_db);
    q.prepare("SELECT t.* FROM tags t JOIN asset_tags at ON t.id = at.tag_id WHERE at.asset_id=?");
    q.addBindValue(assetId);
    if (q.exec()) {
        while (q.next()) {
            Tag t;
            t.id = q.value("id").toInt();
            t.name = q.value("name").toString();
            t.color = QColor(q.value("color").toString());
            tags.append(t);
        }
    }
    return tags;
}

QVector<Asset> DatabaseManager::getAssetsByTag(int tagId) const
{
    QVector<Asset> assets;
    QSqlQuery q(m_db);
    q.prepare("SELECT a.* FROM assets a JOIN asset_tags at ON a.id = at.asset_id WHERE at.tag_id=?");
    q.addBindValue(tagId);
    if (q.exec()) {
        while (q.next()) {
            Asset a;
            a.id = q.value("id").toString();
            a.filePath = q.value("file_path").toString();
            a.fileName = q.value("file_name").toString();
            assets.append(a);
        }
    }
    return assets;
}

bool DatabaseManager::beginTransaction()
{
    return m_db.transaction();
}

bool DatabaseManager::commitTransaction()
{
    return m_db.commit();
}

bool DatabaseManager::rollbackTransaction()
{
    return m_db.rollback();
}
