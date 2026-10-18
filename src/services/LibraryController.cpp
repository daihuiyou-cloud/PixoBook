#include "LibraryController.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "parsers/MetadataParser.h"
#include <QDebug>

LibraryController::LibraryController(IDatabaseManager *db, IImageCache *cache,
                                     FileScanner *scanner, FileWatcher *watcher,
                                     QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_cache(cache)
    , m_scanner(scanner)
    , m_watcher(watcher)
{
    connect(m_scanner, &FileScanner::scanStarted, this, [this]() {
        ++m_scanDepth;
        if (m_db->beginTransaction())
            ++m_scanTxDepth;
        else
            qWarning("scan: beginTransaction failed");
        if (m_scanDepth == 1) {
            // Snapshot existing paths/hashes once so duplicate detection during
            // the scan is O(1) in memory instead of two SQL round-trips per file.
            m_scanKnownPaths.clear();
            m_scanKnownHashes.clear();
            const auto existing = m_db->getAllAssets();
            m_scanKnownPaths.reserve(existing.size());
            m_scanKnownHashes.reserve(existing.size());
            for (const auto &a : existing) {
                m_scanKnownPaths.insert(a.filePath);
                if (!a.hash.isEmpty())
                    m_scanKnownHashes.insert(a.hash);
            }
        }
    });

    connect(m_scanner, &FileScanner::assetFound, this, [this](const ScannedAsset &item) {
        const Asset &asset = item.asset;
        if (m_scanDepth > 0) {
            if (m_scanKnownPaths.contains(asset.filePath))
                return;
            if (!asset.hash.isEmpty() && m_scanKnownHashes.contains(asset.hash))
                return;
        } else if (!m_db->findByPath(asset.filePath).id.isEmpty()
                   || !m_db->findByHash(asset.hash).id.isEmpty()) {
            return;
        }
        if (!m_db->insertAsset(asset)) {
            qWarning() << "scan: insertAsset failed for" << asset.filePath;
            return;
        }
        if (m_scanDepth > 0) {
            m_scanKnownPaths.insert(asset.filePath);
            if (!asset.hash.isEmpty())
                m_scanKnownHashes.insert(asset.hash);
        }
        if (!item.metadata.source.isEmpty()) {
            Metadata meta = item.metadata;
            meta.assetId = asset.id;
            (void)m_db->upsertMetadata(meta);
        }
    });

    connect(m_scanner, &FileScanner::scanProgress, this, &LibraryController::scanProgress);
    connect(m_scanner, &FileScanner::scanFinished, this, [this]() {
        if (m_scanTxDepth > 0) {
            if (!m_db->commitTransaction()) {
                qWarning("scan: commitTransaction failed");
                (void)m_db->rollbackTransaction();
            }
            --m_scanTxDepth;
        }
        if (m_scanDepth > 0)
            --m_scanDepth;
        if (m_scanDepth == 0) {
            m_scanKnownPaths.clear();
            m_scanKnownHashes.clear();
        }
        emit assetCountChanged(m_db->countAssets());
        emit scanFinished();
        emit dataChanged();
    });

    connect(m_watcher, &FileWatcher::fileAdded, this, [this](const QString &path) {
        if (FileScanner::isSupportedFormat(path)) {
            scanAndInsertFile(path);
            emit dataChanged();
        }
    });

    connect(m_watcher, &FileWatcher::fileRemoved, this, [this](const QString &path) {
        Asset a = m_db->findByPath(path);
        if (!a.id.isEmpty()) {
            (void)m_db->deleteAsset(a.id);
            m_scanKnownPaths.remove(path);
            if (!a.hash.isEmpty())
                m_scanKnownHashes.remove(a.hash);
            emit dataChanged();
        }
    });
}

LibraryController::~LibraryController() = default;

QVector<Asset> LibraryController::loadAssets(const QString &keyword, const QString &source,
                                              const QVector<int> &tagIds, bool onlyFavorites,
                                              const QString &sortField, bool sortAscending,
                                              int offset, int limit,
                                              int *outTotalCount) const
{
    return m_db->searchAssets(keyword, source, tagIds, onlyFavorites, sortField, sortAscending,
                              offset, limit, outTotalCount);
}

Asset LibraryController::getAsset(const QString &id) const
{
    return m_db->getAsset(id);
}

Metadata LibraryController::getMetadata(const QString &assetId) const
{
    return m_db->getMetadata(assetId);
}

QVector<Tag> LibraryController::getTagsForAsset(const QString &assetId) const
{
    return m_db->getTagsForAsset(assetId);
}

bool LibraryController::updateMetadata(const Metadata &metadata)
{
    bool ok = m_db->upsertMetadata(metadata);
    if (ok)
        emit dataChanged();
    return ok;
}

QVector<Tag> LibraryController::getAllTags() const
{
    return m_db->getAllTags();
}

Tag LibraryController::getTag(int tagId) const
{
    return m_db->getTag(tagId);
}

int LibraryController::createTag(const QString &name, const QColor &color)
{
    Tag tag;
    tag.name = name;
    tag.color = color;
    int id = m_db->insertTag(tag);
    if (id >= 0)
        emit tagsChanged();
    return id;
}

bool LibraryController::renameTag(int tagId, const QString &newName)
{
    Tag t = m_db->getTag(tagId);
    if (!t.isValid()) return false;
    t.name = newName;
    bool ok = m_db->updateTag(t);
    if (ok)
        emit tagsChanged();
    return ok;
}

bool LibraryController::deleteTag(int tagId)
{
    bool ok = m_db->deleteTag(tagId);
    if (ok)
        emit tagsChanged();
    return ok;
}

void LibraryController::addTagToAssets(const QVector<QString> &assetIds, int tagId)
{
    if (!m_db->beginTransaction()) {
        qWarning("addTagToAssets: beginTransaction failed");
        return;
    }
    for (const auto &aid : assetIds) {
        if (!m_db->addTagToAsset(aid, tagId)) {
            (void)m_db->rollbackTransaction();
            return;
        }
    }
    if (!m_db->commitTransaction()) {
        qWarning("addTagToAssets: commitTransaction failed");
        (void)m_db->rollbackTransaction();
        return;
    }
    emit dataChanged();
}

void LibraryController::removeTagFromAsset(const QString &assetId, int tagId)
{
    if (m_db->removeTagFromAsset(assetId, tagId))
        emit dataChanged();
}

void LibraryController::addFolder(const QString &dir)
{
    if (!m_folders.contains(dir)) {
        m_folders.append(dir);
    }
    m_watcher->addWatchPath(dir);
}

void LibraryController::scanFolder(const QString &dir)
{
    addFolder(dir);
    m_cache->invalidateDir(dir);
    m_scanner->scanDirectory(dir, true);
}

void LibraryController::removeFolder(const QString &dir)
{
    m_folders.removeAll(dir);
    m_watcher->removeWatchPath(dir);
    m_cache->invalidateDir(dir);
    emit dataChanged();
}

void LibraryController::scanAndInsertFile(const QString &path)
{
    Asset asset = FileScanner::scanSingleFile(path);
    if (asset.id.isEmpty()) return;
    if (m_scanDepth > 0 && (m_scanKnownPaths.contains(asset.filePath)
                            || (!asset.hash.isEmpty() && m_scanKnownHashes.contains(asset.hash)))) {
        return;
    }
    if (!m_db->findByHash(asset.hash).id.isEmpty()) return;

    if (!m_db->insertAsset(asset)) {
        qWarning() << "scanAndInsertFile: insertAsset failed for" << asset.filePath;
        return;
    }
    if (m_scanDepth > 0) {
        m_scanKnownPaths.insert(asset.filePath);
        if (!asset.hash.isEmpty())
            m_scanKnownHashes.insert(asset.hash);
    }
    Metadata meta = MetadataParser::parse(asset.filePath);
    if (!meta.source.isEmpty()) {
        meta.assetId = asset.id;
        (void)m_db->upsertMetadata(meta);
    }
}

bool LibraryController::deleteAssets(const QVector<Asset> &assets)
{
    if (!m_db->beginTransaction()) {
        qWarning("deleteAssets: beginTransaction failed");
        return false;
    }
    for (const auto &a : assets) {
        if (!m_db->deleteAsset(a.id)) {
            (void)m_db->rollbackTransaction();
            emit dataChanged();
            return false;
        }
    }
    if (!m_db->commitTransaction()) {
        qWarning("deleteAssets: commitTransaction failed");
        (void)m_db->rollbackTransaction();
        emit dataChanged();
        return false;
    }
    emit dataChanged();
    return true;
}

void LibraryController::toggleFavorite(const QString &assetId, bool isFavorite)
{
    (void)m_db->updateAssetFavorite(assetId, isFavorite);
    emit dataChanged();
}

int LibraryController::countAssets(const QString &keyword, const QString &source,
                                    const QVector<int> &tagIds, bool onlyFavorites) const
{
    return m_db->countAssets(keyword, source, tagIds, onlyFavorites);
}
