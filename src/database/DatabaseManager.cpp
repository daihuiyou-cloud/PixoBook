#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {
constexpr int kSchemaVersion = 3;
}

DatabaseManager::DatabaseManager(const QString &dbPath)
    : m_dbPath(dbPath)
{
    m_connectionName = QStringLiteral("pixobook_%1").arg(dbPath);
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
    // Drop our reference before removeDatabase(); otherwise Qt warns that the
    // connection is still in use and may leave the driver registered.
    m_db = QSqlDatabase();
    if (QSqlDatabase::connectionNames().contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

namespace {
QDateTime parseIsoDt(const QString &s, Qt::DateFormat fmt = Qt::ISODate)
{
    auto dt = QDateTime::fromString(s, fmt);
    return dt.isValid() ? dt : QDateTime();
}

// Asset.filePath always uses '/' separators (QFileInfo::absoluteFilePath).
QString normalizeFolderPath(const QString &path)
{
    QString p = QDir::fromNativeSeparators(path);
    while (p.size() > 1 && p.endsWith('/'))
        p.chop(1);
    return p;
}

QString escapeLikePattern(const QString &value)
{
    QString escaped;
    escaped.reserve(value.size());
    for (const QChar c : value) {
        if (c == '\\' || c == '%' || c == '_')
            escaped += '\\';
        escaped += c;
    }
    return escaped;
}

Asset readAssetRow(const QSqlQuery &q)
{
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
    a.missingSince = parseIsoDt(q.value("missing_since").toString(), Qt::ISODate);
    // Only present when the query joined metadata, as searchAssets() does.
    a.metadataSource = q.value("metadata_source").toString();
    a.metadataPrompt = q.value("metadata_prompt").toString();
    return a;
}
}

bool DatabaseManager::initialize()
{
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    if (!createSchema())
        return false;
    return migrate();
}

bool DatabaseManager::createSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec("PRAGMA journal_mode=WAL"))
        qWarning("Failed to enable WAL mode: %s", qPrintable(q.lastError().text()));
    if (!q.exec("PRAGMA foreign_keys=ON"))
        qWarning("Failed to enable foreign keys: %s", qPrintable(q.lastError().text()));

    bool ok = true;
    auto execSchema = [&](const char *sql) {
        if (!q.exec(QString::fromLatin1(sql))) {
            qWarning() << "Schema statement failed:" << q.lastError().text();
            ok = false;
        }
    };

    execSchema(
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
        "  updated_at TEXT NOT NULL,"
        "  missing_since TEXT NOT NULL DEFAULT ''"
        ")"
    );

    execSchema(
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

    execSchema(
        "CREATE TABLE IF NOT EXISTS tags ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT UNIQUE NOT NULL,"
        "  color TEXT NOT NULL DEFAULT '#888888'"
        ")"
    );

    execSchema(
        "CREATE TABLE IF NOT EXISTS asset_tags ("
        "  asset_id TEXT NOT NULL,"
        "  tag_id INTEGER NOT NULL,"
        "  PRIMARY KEY (asset_id, tag_id),"
        "  FOREIGN KEY (asset_id) REFERENCES assets(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
        ")"
    );

    execSchema("CREATE INDEX IF NOT EXISTS idx_assets_hash ON assets(hash)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_assets_folder ON assets(folder_id)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_assets_favorite ON assets(is_favorite)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_assets_created ON assets(created_at)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_metadata_source ON metadata(source)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_asset_tags_tag ON asset_tags(tag_id)");
    execSchema("CREATE INDEX IF NOT EXISTS idx_metadata_asset_id ON metadata(asset_id)");

    execSchema(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER PRIMARY KEY"
        ")"
    );

    return ok;
}

bool DatabaseManager::migrate()
{
    QSqlQuery q(m_db);
    if (!q.exec("SELECT MAX(version) FROM schema_version")) {
        qWarning() << "Failed to read schema version:" << q.lastError().text();
        return false;
    }
    int version = 0;
    if (q.next())
        version = q.value(0).toInt();

    if (version > 0 && version < kSchemaVersion)
        backupBeforeMigration(version);

    auto setVersion = [&](int v) {
        QSqlQuery update(m_db);
        update.prepare("INSERT OR REPLACE INTO schema_version (version) VALUES (?)");
        update.addBindValue(v);
        if (!update.exec()) {
            qWarning() << "Failed to persist schema version" << v << ":"
                       << update.lastError().text();
            return false;
        }
        return true;
    };

    if (version < 1) {
        // v1: initial schema
        if (!setVersion(1))
            return false;
        version = 1;
    }

    if (version < 2) {
        // v2: future migration placeholder
        // q.exec("ALTER TABLE assets ADD COLUMN ...");
        if (!setVersion(2))
            return false;
        version = 2;
    }

    if (version < 3) {
        // v3: a file that disappears is marked missing, not deleted. The
        // column is also declared in createSchema(), so guard the ALTER for
        // databases created after this version.
        if (!hasAssetColumn(QStringLiteral("missing_since"))) {
            QSqlQuery alter(m_db);
            if (!alter.exec("ALTER TABLE assets ADD COLUMN missing_since TEXT NOT NULL DEFAULT ''")) {
                qWarning() << "Migration v3 failed:" << alter.lastError().text();
                return false;
            }
        }
        // Partial index on the (usually tiny) missing set. Created here rather
        // than in createSchema() because an old database only gains the column
        // during this migration.
        QSqlQuery index(m_db);
        if (!index.exec("CREATE INDEX IF NOT EXISTS idx_assets_missing "
                        "ON assets(missing_since) WHERE missing_since <> ''")) {
            qWarning() << "Migration v3 index failed:" << index.lastError().text();
            return false;
        }
        if (!setVersion(3))
            return false;
    }
    return true;
}

bool DatabaseManager::hasAssetColumn(const QString &column) const
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(assets)"))) {
        qWarning() << "PRAGMA table_info failed:" << q.lastError().text();
        return false;
    }
    while (q.next()) {
        if (q.value(1).toString() == column)
            return true;
    }
    return false;
}

void DatabaseManager::backupBeforeMigration(int fromVersion) const
{
    if (m_dbPath.isEmpty() || !QFileInfo::exists(m_dbPath))
        return;
    // Fold the WAL back into the main file so the copy is self-contained.
    QSqlQuery checkpoint(m_db);
    if (!checkpoint.exec("PRAGMA wal_checkpoint(FULL)"))
        qWarning() << "Pre-migration checkpoint failed:" << checkpoint.lastError().text();

    QString backupPath = m_dbPath + QStringLiteral(".bak-v%1").arg(fromVersion);
    if (QFileInfo::exists(backupPath)) {
        backupPath += QLatin1Char('-')
                      + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"));
    }
    if (!QFile::copy(m_dbPath, backupPath))
        qWarning() << "Could not back up database before migration:" << backupPath;
    else
        qInfo() << "Database backed up to" << backupPath;
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
    // Any successful content update means the file is reachable again.
    q.prepare("UPDATE assets SET file_path=?, file_name=?, file_size=?, width=?, height=?, "
              "format=?, hash=?, is_favorite=?, folder_id=?, updated_at=?, "
              "missing_since='' WHERE id=?");
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
    if (q.exec() && q.next())
        a = readAssetRow(q);
    return a;
}

QVector<Asset> DatabaseManager::getAllAssets() const
{
    QVector<Asset> assets;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM assets ORDER BY created_at DESC")) {
        while (q.next())
            assets.append(readAssetRow(q));
    }
    return assets;
}

QVector<Asset> DatabaseManager::searchAssets(const QString &keyword, const QString &source,
                                              const QVector<int> &tagIds, bool onlyFavorites,
                                              const QString &sortField, bool sortAscending,
                                              int offset, int limit,
                                              int *outTotalCount,
                                              const QString &folderPath) const
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
    if (!folderPath.isEmpty()) {
        // Match files directly in the folder and everything below it. LIKE is
        // case-insensitive for ASCII paths on Windows; '%'/'_' are escaped.
        const QString prefix = escapeLikePattern(normalizeFolderPath(folderPath));
        sql += "AND (a.file_path LIKE ? ESCAPE '\\' OR a.file_path LIKE ? ESCAPE '\\') ";
        binds.append(prefix);
        binds.append(prefix + "/%");
    }
    // Validate sort field to prevent SQL injection
    QString sortCol = "a.created_at";
    if (sortField == QLatin1String("file_name")) sortCol = "a.file_name";
    else if (sortField == QLatin1String("file_size")) sortCol = "a.file_size";

    // id is a stable tie-breaker: without it, rows sharing the same sort key
    // (e.g. identical created_at) can move between LIMIT/OFFSET pages.
    if (outTotalCount) {
        int dot = sortCol.indexOf('.');
        QString sortColOuter = dot >= 0 ? sortCol.mid(dot + 1) : sortCol;
        sql = "SELECT *, COUNT(*) OVER() AS _total_count FROM (" + sql + ") AS _inner "
              "ORDER BY " + sortColOuter + " " + (sortAscending ? "ASC" : "DESC")
              + ", id ASC";
    } else {
        sql += "ORDER BY " + sortCol + " " + (sortAscending ? "ASC" : "DESC")
               + ", a.id ASC";
    }

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
        bool gotCount = false;
        while (q.next()) {
            assets.append(readAssetRow(q));
            if (outTotalCount && !gotCount) {
                *outTotalCount = q.value("_total_count").toInt();
                gotCount = true;
            }
        }
        if (outTotalCount && !gotCount)
            *outTotalCount = 0;
    }
    return assets;
}

int DatabaseManager::countAssets(const QString &keyword, const QString &source,
                                  const QVector<int> &tagIds, bool onlyFavorites,
                                  const QString &folderPath) const
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
    if (!folderPath.isEmpty()) {
        const QString prefix = escapeLikePattern(normalizeFolderPath(folderPath));
        sql += "AND (a.file_path LIKE ? ESCAPE '\\' OR a.file_path LIKE ? ESCAPE '\\') ";
        binds.append(prefix);
        binds.append(prefix + "/%");
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
    if (q.exec() && q.next())
        a = readAssetRow(q);
    return a;
}

Asset DatabaseManager::findByHash(const QString &hash) const
{
    Asset a;
    if (hash.isEmpty()) return a;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM assets WHERE hash=?");
    q.addBindValue(hash);
    if (q.exec() && q.next())
        a = readAssetRow(q);
    return a;
}

bool DatabaseManager::markAssetMissing(const QString &filePath, const QDateTime &when)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assets SET missing_since=?, updated_at=? WHERE file_path=?");
    q.addBindValue(when.toString(Qt::ISODate));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(filePath);
    if (!q.exec()) {
        qWarning() << "markAssetMissing failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool DatabaseManager::markAssetPresent(const QString &assetId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assets SET missing_since='' WHERE id=?");
    q.addBindValue(assetId);
    if (!q.exec()) {
        qWarning() << "markAssetPresent failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

int DatabaseManager::countMissingAssets() const
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM assets WHERE missing_since <> ''") && q.next())
        return q.value(0).toInt();
    return 0;
}

QVector<Asset> DatabaseManager::getMissingAssets() const
{
    QVector<Asset> assets;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM assets WHERE missing_since <> '' ORDER BY missing_since ASC")) {
        while (q.next())
            assets.append(readAssetRow(q));
    }
    return assets;
}

bool DatabaseManager::deleteMissingAssets()
{
    QSqlQuery q(m_db);
    if (!q.exec("DELETE FROM assets WHERE missing_since <> ''")) {
        qWarning() << "deleteMissingAssets failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QVector<Asset> DatabaseManager::getAnnotatedAssets() const
{
    QVector<Asset> assets;
    QSqlQuery q(m_db);
    const char *sql =
        "SELECT a.* FROM assets a "
        "WHERE a.is_favorite = 1 "
        "   OR EXISTS (SELECT 1 FROM asset_tags at WHERE at.asset_id = a.id) "
        "   OR EXISTS (SELECT 1 FROM metadata m WHERE m.asset_id = a.id) "
        "ORDER BY a.created_at DESC, a.id ASC";
    if (q.exec(QString::fromLatin1(sql))) {
        while (q.next())
            assets.append(readAssetRow(q));
    }
    return assets;
}

QHash<QString, Metadata> DatabaseManager::getAllMetadata() const
{
    QHash<QString, Metadata> metadata;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM metadata")) {
        while (q.next()) {
            Metadata m;
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
            metadata.insert(m.assetId, m);
        }
    }
    return metadata;
}

QHash<QString, QVector<Tag>> DatabaseManager::getAllAssetTags() const
{
    QHash<QString, QVector<Tag>> byAsset;
    QSqlQuery q(m_db);
    if (q.exec("SELECT at.asset_id, t.id, t.name, t.color FROM asset_tags at "
               "JOIN tags t ON t.id = at.tag_id ORDER BY t.name")) {
        while (q.next()) {
            Tag t;
            t.id = q.value("id").toInt();
            t.name = q.value("name").toString();
            t.color = QColor(q.value("color").toString());
            byAsset[q.value("asset_id").toString()].append(t);
        }
    }
    return byAsset;
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

Tag DatabaseManager::getTag(int tagId) const
{
    Tag t;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM tags WHERE id=?");
    q.addBindValue(tagId);
    if (q.exec() && q.next()) {
        t.id = q.value("id").toInt();
        t.name = q.value("name").toString();
        t.color = QColor(q.value("color").toString());
    }
    return t;
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
        while (q.next())
            assets.append(readAssetRow(q));
    }
    return assets;
}

bool DatabaseManager::beginTransaction()
{
    if (m_txDepth == 0) {
        if (!m_db.transaction())
            return false;
        m_txDepth = 1;
        return true;
    }
    QString savepoint = QStringLiteral("pixobook_sp_%1").arg(m_txDepth);
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SAVEPOINT %1").arg(savepoint)))
        return false;
    m_txDepth++;
    return true;
}

bool DatabaseManager::commitTransaction()
{
    if (m_txDepth <= 0)
        return false;
    if (m_txDepth > 1) {
        QString savepoint = QStringLiteral("pixobook_sp_%1").arg(m_txDepth - 1);
        QSqlQuery q(m_db);
        m_txDepth--;
        return q.exec(QStringLiteral("RELEASE %1").arg(savepoint));
    }
    m_txDepth = 0;
    return m_db.commit();
}

bool DatabaseManager::rollbackTransaction()
{
    if (m_txDepth <= 0)
        return false;
    if (m_txDepth > 1) {
        QString savepoint = QStringLiteral("pixobook_sp_%1").arg(m_txDepth - 1);
        QSqlQuery q(m_db);
        m_txDepth--;
        return q.exec(QStringLiteral("ROLLBACK TO %1").arg(savepoint))
               && q.exec(QStringLiteral("RELEASE %1").arg(savepoint));
    }
    m_txDepth = 0;
    return m_db.rollback();
}
