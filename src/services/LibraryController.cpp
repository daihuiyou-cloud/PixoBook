#include "LibraryController.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "parsers/MetadataParser.h"

LibraryController::LibraryController(IDatabaseManager *db, IImageCache *cache,
                                     FileScanner *scanner, FileWatcher *watcher,
                                     QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_cache(cache)
    , m_scanner(scanner)
    , m_watcher(watcher)
{
    connect(m_scanner, &FileScanner::assetFound, this, [this](const Asset &asset) {
        Asset existing = m_db->findByPath(asset.filePath);
        if (existing.id.isEmpty() && m_db->findByHash(asset.hash).id.isEmpty()) {
            m_db->insertAsset(asset);
            Metadata meta = MetadataParser::parse(asset.filePath);
            if (!meta.source.isEmpty()) {
                meta.assetId = asset.id;
                m_db->upsertMetadata(meta);
            }
        }
    });

    connect(m_scanner, &FileScanner::scanProgress, this, &LibraryController::scanProgress);
    connect(m_scanner, &FileScanner::scanFinished, this, [this]() {
        emit assetCountChanged(m_db->getAllAssets().size());
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
            m_db->deleteAsset(a.id);
            emit dataChanged();
        }
    });
}

LibraryController::~LibraryController() = default;

QVector<Asset> LibraryController::loadAssets(const QString &keyword, const QString &source,
                                              const QVector<int> &tagIds, bool onlyFavorites,
                                              const QString &sortField, bool sortAscending,
                                              int offset, int limit)
{
    return m_db->searchAssets(keyword, source, tagIds, onlyFavorites, sortField, sortAscending, offset, limit);
}

Asset LibraryController::getAsset(const QString &id)
{
    return m_db->getAsset(id);
}

Metadata LibraryController::getMetadata(const QString &assetId)
{
    return m_db->getMetadata(assetId);
}

QVector<Tag> LibraryController::getTagsForAsset(const QString &assetId)
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

QVector<Tag> LibraryController::getAllTags()
{
    return m_db->getAllTags();
}

int LibraryController::createTag(const QString &name, const QColor &color)
{
    Tag tag;
    tag.name = name;
    tag.color = color;
    return m_db->insertTag(tag);
}

bool LibraryController::renameTag(int tagId, const QString &newName)
{
    auto allTags = m_db->getAllTags();
    for (auto &t : allTags) {
        if (t.id == tagId) {
            t.name = newName;
            return m_db->updateTag(t);
        }
    }
    return false;
}

bool LibraryController::deleteTag(int tagId)
{
    return m_db->deleteTag(tagId);
}

void LibraryController::addTagToAssets(const QVector<QString> &assetIds, int tagId)
{
    m_db->beginTransaction();
    for (const auto &aid : assetIds) {
        if (!m_db->addTagToAsset(aid, tagId)) {
            m_db->rollbackTransaction();
            return;
        }
    }
    m_db->commitTransaction();
}

void LibraryController::removeTagFromAsset(const QString &assetId, int tagId)
{
    m_db->removeTagFromAsset(assetId, tagId);
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
    emit dataChanged();
}

void LibraryController::scanAndInsertFile(const QString &path)
{
    Asset asset = FileScanner::scanSingleFile(path);
    if (asset.id.isEmpty()) return;
    if (!m_db->findByHash(asset.hash).id.isEmpty()) return;

    m_db->insertAsset(asset);
    Metadata meta = MetadataParser::parse(asset.filePath);
    if (!meta.source.isEmpty()) {
        meta.assetId = asset.id;
        m_db->upsertMetadata(meta);
    }
}

bool LibraryController::deleteAssets(const QVector<Asset> &assets)
{
    m_db->beginTransaction();
    for (const auto &a : assets) {
        if (!m_db->deleteAsset(a.id)) {
            m_db->rollbackTransaction();
            emit dataChanged();
            return false;
        }
    }
    m_db->commitTransaction();
    emit dataChanged();
    return true;
}

void LibraryController::toggleFavorite(const QString &assetId, bool isFavorite)
{
    m_db->updateAssetFavorite(assetId, isFavorite);
    emit dataChanged();
}

int LibraryController::countAssets(const QString &keyword, const QString &source,
                                    const QVector<int> &tagIds, bool onlyFavorites)
{
    return m_db->countAssets(keyword, source, tagIds, onlyFavorites);
}
