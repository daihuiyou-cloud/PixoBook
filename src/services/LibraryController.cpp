#include "LibraryController.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "parsers/MetadataParser.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QtConcurrent>

namespace {
bool isUnderAnyFolder(const QString &path, const QStringList &roots)
{
    const QString p = QDir::fromNativeSeparators(path);
    for (const auto &root : roots) {
        QString r = QDir::fromNativeSeparators(root);
        while (r.size() > 1 && r.endsWith(QLatin1Char('/')))
            r.chop(1);
        if (p.compare(r, Qt::CaseInsensitive) == 0
            || p.startsWith(r + QLatin1Char('/'), Qt::CaseInsensitive))
            return true;
    }
    return false;
}
}

LibraryController::LibraryController(IDatabaseManager *db, IImageCache *cache,
                                     FileScanner *scanner, FileWatcher *watcher,
                                     QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_cache(cache)
    , m_scanner(scanner)
    , m_watcher(watcher)
{
    // Give the watcher a moment to deliver removals before stat-ing the tree.
    m_reconcileTimer.setSingleShot(true);
    m_reconcileTimer.setInterval(1000);
    connect(&m_reconcileTimer, &QTimer::timeout,
            this, &LibraryController::reconcileMissingFiles);

    connect(m_scanner, &FileScanner::scanStarted, this, [this]() {
        ++m_scanDepth;
        if (m_db->beginTransaction())
            ++m_scanTxDepth;
        else
            qWarning("scan: beginTransaction failed");
        if (m_scanDepth == 1) {
            // Build the search index once at the end of the scan instead of
            // updating it per inserted row.
            m_db->setDeferSearchIndexing(true);
            // Snapshot existing paths/hashes once so duplicate detection during
            // the scan is O(1) in memory instead of two SQL round-trips per file.
            m_scanKnownPaths.clear();
            m_scanKnownHashes.clear();
            m_scanMissingAssets.clear();
            const auto existing = m_db->getAllAssets();
            m_scanKnownPaths.reserve(existing.size());
            m_scanKnownHashes.reserve(existing.size());
            for (const auto &a : existing) {
                if (a.isMissing()) {
                    // Do not mark the path known: the scanner must visit it so
                    // a file that came back can be restored (tags preserved).
                    m_scanMissingAssets.insert(a.filePath, a);
                } else {
                    m_scanKnownPaths.insert(a.filePath);
                }
                if (!a.hash.isEmpty())
                    m_scanKnownHashes.insert(a.hash);
            }
        }
    });

    connect(m_scanner, &FileScanner::assetFound, this, [this](const ScannedAsset &item) {
        const Asset &asset = item.asset;
        if (m_scanDepth > 0) {
            const auto missing = m_scanMissingAssets.constFind(asset.filePath);
            if (missing != m_scanMissingAssets.constEnd()) {
                (void)restoreMissingAsset(missing.value(), asset, item.metadata);
                return;
            }
            if (m_scanKnownPaths.contains(asset.filePath))
                return;
            if (!asset.hash.isEmpty() && m_scanKnownHashes.contains(asset.hash)) {
                (void)adoptRenamedAsset(asset);
                return;
            }
        } else {
            if (!m_db->findByPath(asset.filePath).id.isEmpty())
                return;
            if (!m_db->findByHash(asset.hash).id.isEmpty()) {
                (void)adoptRenamedAsset(asset);
                return;
            }
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
            m_scanMissingAssets.clear();
            m_db->setDeferSearchIndexing(false);
            if (!m_db->indexPendingAssets())
                qWarning("scan: search index flush failed");
            // Files deleted while the app was closed are not covered by the
            // watcher; reconcile once the scan settles.
            m_reconcileTimer.start();
        }
        emit assetCountChanged(m_db->countAssets());
        emit scanFinished();
        emit dataChanged();
    });

    connect(m_watcher, &FileWatcher::fileAdded, this, [this](const QString &path) {
        if (FileScanner::isSupportedFormat(path)) {
            (void)scanAndInsertFile(path);
            emit dataChanged();
        }
    });

    connect(m_watcher, &FileWatcher::fileRemoved, this, [this](const QString &path) {
        // A disappearing file is a state change, not a delete request. The row
        // and its annotations stay until the user cleans missing records up;
        // a rescan or a rename brings the same row back.
        const Asset a = m_db->findByPath(path);
        if (a.id.isEmpty())
            return;
        if (m_db->markAssetMissing(path, QDateTime::currentDateTime())) {
            if (m_scanDepth > 0) {
                m_scanKnownPaths.remove(path);
                m_scanMissingAssets.insert(path, a);
            }
            emit dataChanged();
        }
    });
}

LibraryController::~LibraryController() = default;

QVector<Asset> LibraryController::loadAssets(const QString &keyword, const QString &source,
                                              const QVector<int> &tagIds, bool onlyFavorites,
                                              const QString &sortField, bool sortAscending,
                                              int offset, int limit,
                                              int *outTotalCount,
                                              const QString &folderPath) const
{
    return m_db->searchAssets(keyword, source, tagIds, onlyFavorites, sortField, sortAscending,
                              offset, limit, outTotalCount, folderPath);
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
    // Give the scanner the current path set so it can skip hash/metadata work
    // for files that are already imported. The full scan snapshot in
    // scanStarted() still handles hash de-duplication for new files.
    QSet<QString> knownPaths;
    const auto existing = m_db->getAllAssets();
    knownPaths.reserve(existing.size());
    for (const auto &a : existing) {
        // Missing paths are rescanned so a returning file is restored rather
        // than skipped as "already imported".
        if (!a.isMissing())
            knownPaths.insert(a.filePath);
    }
    m_scanner->scanDirectory(dir, true, knownPaths);
}

void LibraryController::removeFolder(const QString &dir)
{
    m_folders.removeAll(dir);
    m_watcher->removeWatchPath(dir);
    m_cache->invalidateDir(dir);
    emit dataChanged();
}

bool LibraryController::scanAndInsertFile(const QString &path)
{
    Asset asset = FileScanner::scanSingleFile(path);
    if (asset.id.isEmpty()) return false;
    if (m_scanDepth > 0) {
        const auto missing = m_scanMissingAssets.constFind(asset.filePath);
        if (missing != m_scanMissingAssets.constEnd()) {
            (void)restoreMissingAsset(missing.value(), asset, MetadataParser::parse(path));
            return false;
        }
        if (m_scanKnownPaths.contains(asset.filePath))
            return false;
        if (!asset.hash.isEmpty() && m_scanKnownHashes.contains(asset.hash)) {
            (void)adoptRenamedAsset(asset);
            return false;
        }
    } else {
        const Asset existing = m_db->findByPath(asset.filePath);
        if (!existing.id.isEmpty()) {
            if (existing.isMissing())
                (void)restoreMissingAsset(existing, asset, MetadataParser::parse(path));
            return false;
        }
        if (!m_db->findByHash(asset.hash).id.isEmpty()) {
            (void)adoptRenamedAsset(asset);
            return false;
        }
    }

    if (!m_db->insertAsset(asset)) {
        qWarning() << "scanAndInsertFile: insertAsset failed for" << asset.filePath;
        return false;
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
    return true;
}

bool LibraryController::adoptRenamedAsset(const Asset &scanned)
{
    if (scanned.hash.isEmpty())
        return false;
    const Asset existing = m_db->findByHash(scanned.hash);
    if (existing.id.isEmpty() || existing.filePath == scanned.filePath)
        return false;
    // Only treat it as a move/rename when the old path is really gone. A copy
    // imported next to the original must stay a duplicate, not steal its row.
    if (existing.filePath.isEmpty() || QFileInfo::exists(existing.filePath))
        return false;

    Asset updated = existing;
    updated.filePath = scanned.filePath;
    updated.fileName = scanned.fileName;
    updated.fileSize = scanned.fileSize;
    updated.width = scanned.width;
    updated.height = scanned.height;
    updated.format = scanned.format;
    updated.folderId = scanned.folderId;
    updated.updatedAt = scanned.updatedAt;
    if (!m_db->updateAsset(updated)) {
        qWarning() << "adoptRenamedAsset: updateAsset failed for" << scanned.filePath;
        return false;
    }
    if (m_scanDepth > 0) {
        m_scanMissingAssets.remove(existing.filePath);
        m_scanKnownPaths.remove(existing.filePath);
        m_scanKnownPaths.insert(updated.filePath);
    }
    return true;
}

bool LibraryController::restoreMissingAsset(const Asset &existing, const Asset &scanned,
                                            const Metadata &metadata)
{
    Asset updated = existing;
    updated.filePath = scanned.filePath;
    updated.fileName = scanned.fileName;
    updated.fileSize = scanned.fileSize;
    updated.width = scanned.width;
    updated.height = scanned.height;
    updated.format = scanned.format;
    updated.hash = scanned.hash;
    updated.folderId = scanned.folderId;
    updated.updatedAt = QDateTime::currentDateTime();
    updated.missingSince = QDateTime();
    if (!m_db->updateAsset(updated)) {
        qWarning() << "restoreMissingAsset: updateAsset failed for" << scanned.filePath;
        return false;
    }
    if (!metadata.source.isEmpty()) {
        Metadata meta = metadata;
        meta.assetId = existing.id;
        (void)m_db->upsertMetadata(meta);
    }
    if (m_scanDepth > 0) {
        m_scanMissingAssets.remove(existing.filePath);
        m_scanKnownPaths.insert(updated.filePath);
        if (!updated.hash.isEmpty())
            m_scanKnownHashes.insert(updated.hash);
    }
    return true;
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

void LibraryController::reconcileMissingFiles()
{
    if (m_reconcileRunning || m_folders.isEmpty())
        return;

    // Only rows under watched folders are checked: an asset from a folder the
    // user stopped watching may live on a drive that is currently unplugged,
    // and must not be marked missing just because we are not watching it.
    const auto assets = m_db->getAllAssets();
    QVector<QPair<QString, QString>> candidates;
    candidates.reserve(assets.size());
    for (const auto &a : assets) {
        if (a.isMissing() || a.filePath.isEmpty())
            continue;
        if (!isUnderAnyFolder(a.filePath, m_folders))
            continue;
        candidates.append(qMakePair(a.id, a.filePath));
    }
    if (candidates.isEmpty())
        return;

    m_reconcileRunning = true;
    QPointer<LibraryController> guard(this);
    QtConcurrent::run([guard, candidates]() {
        QStringList missing;
        missing.reserve(candidates.size());
        for (const auto &candidate : candidates) {
            if (!QFileInfo::exists(candidate.second))
                missing.append(candidate.second);
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, missing]() {
            if (!guard)
                return;
            guard->m_reconcileRunning = false;
            if (missing.isEmpty()) {
                emit guard->missingCheckFinished(0);
                return;
            }
            if (!guard->m_db->beginTransaction()) {
                emit guard->missingCheckFinished(0);
                return;
            }
            int marked = 0;
            const QDateTime now = QDateTime::currentDateTime();
            for (const auto &path : missing) {
                if (guard->m_db->markAssetMissing(path, now))
                    ++marked;
            }
            if (!guard->m_db->commitTransaction()) {
                (void)guard->m_db->rollbackTransaction();
                marked = 0;
            }
            emit guard->missingCheckFinished(marked);
            if (marked > 0)
                emit guard->dataChanged();
        }, Qt::QueuedConnection);
    });
}

int LibraryController::countMissingAssets() const
{
    return m_db->countMissingAssets();
}

bool LibraryController::cleanupMissingAssets()
{
    if (!m_db->deleteMissingAssets())
        return false;
    m_scanMissingAssets.clear();
    emit dataChanged();
    return true;
}

bool LibraryController::exportAnnotations(const QString &filePath,
                                          QString *errorMessage) const
{
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    const auto assets = m_db->getAnnotatedAssets();
    const auto metadata = m_db->getAllMetadata();
    const auto tags = m_db->getAllAssetTags();

    QJsonArray assetArray;
    for (const auto &a : assets) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), a.id);
        obj.insert(QStringLiteral("filePath"), a.filePath);
        obj.insert(QStringLiteral("fileName"), a.fileName);
        obj.insert(QStringLiteral("hash"), a.hash);
        obj.insert(QStringLiteral("isFavorite"), a.isFavorite);
        obj.insert(QStringLiteral("missingSince"),
                   a.isMissing() ? a.missingSince.toString(Qt::ISODate) : QString());

        QJsonArray tagArray;
        const auto assetTags = tags.value(a.id);
        for (const auto &t : assetTags) {
            QJsonObject tagObj;
            tagObj.insert(QStringLiteral("name"), t.name);
            tagObj.insert(QStringLiteral("color"), t.color.name());
            tagArray.append(tagObj);
        }
        obj.insert(QStringLiteral("tags"), tagArray);

        const auto metaIt = metadata.constFind(a.id);
        if (metaIt != metadata.constEnd()) {
            const Metadata &m = metaIt.value();
            QJsonObject metaObj;
            metaObj.insert(QStringLiteral("source"), m.source);
            metaObj.insert(QStringLiteral("prompt"), m.prompt);
            metaObj.insert(QStringLiteral("negativePrompt"), m.negativePrompt);
            metaObj.insert(QStringLiteral("seed"), static_cast<double>(m.seed));
            metaObj.insert(QStringLiteral("steps"), m.steps);
            metaObj.insert(QStringLiteral("cfgScale"), m.cfgScale);
            metaObj.insert(QStringLiteral("modelName"), m.modelName);
            metaObj.insert(QStringLiteral("sampler"), m.sampler);
            metaObj.insert(QStringLiteral("rawJson"), m.rawJson);
            obj.insert(QStringLiteral("metadata"), metaObj);
        }
        assetArray.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("pixobook-annotations"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("exportedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("assets"), assetArray);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    const QJsonDocument doc(root);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        const QString error = file.errorString();
        file.cancelWriting();
        return fail(error);
    }
    if (!file.commit())
        return fail(file.errorString());
    return true;
}

bool LibraryController::importAnnotations(const QString &filePath, ImportSummary *summary,
                                          QString *errorMessage)
{
    ImportSummary counts;
    auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        if (summary)
            *summary = counts;
        return false;
    };

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    if (file.size() > 64LL * 1024 * 1024)
        return fail(tr("annotation file is larger than 64 MB"));

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(parseError.errorString());
    if (!doc.isObject())
        return fail(tr("not a PixoBook annotation file"));
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString()
        != QLatin1String("pixobook-annotations"))
        return fail(tr("not a PixoBook annotation file"));

    QHash<QString, int> tagIdByName;
    const auto existingTags = m_db->getAllTags();
    for (const auto &tag : existingTags)
        tagIdByName.insert(tag.name, tag.id);

    if (!m_db->beginTransaction())
        return fail(tr("could not start a transaction"));

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const auto &value : assets) {
        const QJsonObject entry = value.toObject();
        const QString hash = entry.value(QStringLiteral("hash")).toString();
        const QString path = entry.value(QStringLiteral("filePath")).toString();

        Asset asset;
        if (!hash.isEmpty())
            asset = m_db->findByHash(hash);
        if (asset.id.isEmpty() && !path.isEmpty())
            asset = m_db->findByPath(path);
        if (asset.id.isEmpty()) {
            ++counts.unmatched;
            continue;
        }
        ++counts.matched;

        if (entry.value(QStringLiteral("isFavorite")).toBool() && !asset.isFavorite) {
            if (m_db->updateAssetFavorite(asset.id, true))
                ++counts.favoritesSet;
        }

        const QJsonArray tags = entry.value(QStringLiteral("tags")).toArray();
        for (const auto &tagValue : tags) {
            const QJsonObject tagObj = tagValue.toObject();
            const QString name = tagObj.value(QStringLiteral("name")).toString().trimmed();
            if (name.isEmpty())
                continue;
            int tagId = tagIdByName.value(name, -1);
            if (tagId < 0) {
                QColor color(tagObj.value(QStringLiteral("color")).toString());
                if (!color.isValid())
                    color = QColor(QStringLiteral("#888888"));
                Tag tag;
                tag.name = name;
                tag.color = color;
                tagId = m_db->insertTag(tag);
                if (tagId < 0)
                    continue;
                tagIdByName.insert(name, tagId);
                ++counts.tagsCreated;
            }
            (void)m_db->addTagToAsset(asset.id, tagId);
        }

        const QJsonObject metaObj = entry.value(QStringLiteral("metadata")).toObject();
        if (!metaObj.isEmpty() && m_db->getMetadata(asset.id).source.isEmpty()) {
            Metadata meta;
            meta.assetId = asset.id;
            meta.source = metaObj.value(QStringLiteral("source")).toString();
            meta.prompt = metaObj.value(QStringLiteral("prompt")).toString();
            meta.negativePrompt = metaObj.value(QStringLiteral("negativePrompt")).toString();
            meta.seed = static_cast<long long>(
                metaObj.value(QStringLiteral("seed")).toDouble());
            meta.steps = metaObj.value(QStringLiteral("steps")).toInt();
            meta.cfgScale = metaObj.value(QStringLiteral("cfgScale")).toDouble();
            meta.modelName = metaObj.value(QStringLiteral("modelName")).toString();
            meta.sampler = metaObj.value(QStringLiteral("sampler")).toString();
            meta.rawJson = metaObj.value(QStringLiteral("rawJson")).toString();
            if (m_db->upsertMetadata(meta))
                ++counts.metadataFilled;
        }
    }

    if (!m_db->commitTransaction()) {
        (void)m_db->rollbackTransaction();
        return fail(tr("could not commit imported annotations"));
    }

    if (summary)
        *summary = counts;
    if (counts.matched > 0) {
        emit dataChanged();
        if (counts.tagsCreated > 0)
            emit tagsChanged();
    }
    return true;
}

int LibraryController::countAssets(const QString &keyword, const QString &source,
                                    const QVector<int> &tagIds, bool onlyFavorites,
                                    const QString &folderPath) const
{
    return m_db->countAssets(keyword, source, tagIds, onlyFavorites, folderPath);
}
