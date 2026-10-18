#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
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
                              int offset = 0, int limit = -1,
                              int *outTotalCount = nullptr,
                              const QString &folderPath = QString()) const;
    Asset getAsset(const QString &id) const;
    Metadata getMetadata(const QString &assetId) const;
    QVector<Tag> getTagsForAsset(const QString &assetId) const;
    [[nodiscard]] bool updateMetadata(const Metadata &metadata);

    // Tag operations
    QVector<Tag> getAllTags() const;
    Tag getTag(int tagId) const;
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

    // File operations. Returns true when a new asset row was inserted.
    [[nodiscard]] bool scanAndInsertFile(const QString &path);
    [[nodiscard]] bool deleteAssets(const QVector<Asset> &assets);
    void toggleFavorite(const QString &assetId, bool isFavorite);

    // Count
    int countAssets(const QString &keyword = {}, const QString &source = {},
                    const QVector<int> &tagIds = {}, bool onlyFavorites = false,
                    const QString &folderPath = QString()) const;

    // These rows lost their file but keep tags/favorite until the user
    // explicitly cleans them up.
    int countMissingAssets() const;
    [[nodiscard]] bool cleanupMissingAssets();

    // Writes tags, favorites, presence state and parsed metadata as JSON.
    // File bytes are not included: the filesystem stays the source of truth.
    [[nodiscard]] bool exportAnnotations(const QString &filePath,
                                         QString *errorMessage = nullptr) const;

    struct ImportSummary {
        int matched = 0;
        int unmatched = 0;
        int tagsCreated = 0;
        int favoritesSet = 0;
        int metadataFilled = 0;
    };

    // Merge policy: favorites are only turned on, tags are unioned, metadata
    // fills only rows that have none. Assets are matched by hash first, then
    // by path; entries with no local asset are reported as unmatched.
    [[nodiscard]] bool importAnnotations(const QString &filePath,
                                         ImportSummary *summary = nullptr,
                                         QString *errorMessage = nullptr);

    // Stats the files under every watched folder in a background thread and
    // marks rows whose file vanished while the app was closed. Runs after a
    // scan (debounced) and can be called explicitly.
    void reconcileMissingFiles();

    QVector<Asset> getMissingAssets() const;
    // Points an existing row (usually one marked missing) at a new file while
    // keeping its id, tags and favorite; refreshes physical fields and, when
    // the row had no metadata, parses the new file's prompt.
    [[nodiscard]] bool relinkAsset(const QString &assetId, const QString &newPath,
                                   QString *errorMessage = nullptr);

signals:
    void assetCountChanged(int count);
    void scanProgress(int current, int total);
    void scanFinished();
    void dataChanged();
    void tagsChanged();
    // Number of rows marked missing by the last reconcile pass (0 if all good).
    void missingCheckFinished(int markedMissing);

private:
    // A moved/renamed file keeps its old DB row (and tags/metadata) when the
    // new path has the same content hash and the old path no longer exists.
    bool adoptRenamedAsset(const Asset &scanned);
    // The file at `existing.filePath` is visible again: refresh its physical
    // fields, clear the missing marker and keep tags/favorite intact.
    bool restoreMissingAsset(const Asset &existing, const Asset &scanned,
                             const Metadata &metadata);

    IDatabaseManager *m_db;
    IImageCache *m_cache;
    FileScanner *m_scanner;
    FileWatcher *m_watcher;
    QStringList m_folders;

    // While a scan transaction is open, duplicate detection uses these
    // in-memory snapshots instead of two SQL queries per scanned file.
    // Scans can overlap (user imports several folders quickly), so depth is
    // counted and the outer transaction commits only when the last one ends.
    int m_scanDepth = 0;
    int m_scanTxDepth = 0;
    QSet<QString> m_scanKnownPaths;
    QSet<QString> m_scanKnownHashes;
    QTimer m_reconcileTimer;
    bool m_reconcileRunning = false;
    // Missing rows are rescanned (their paths are not "known") so a returning
    // file can be restored instead of duplicated.
    QHash<QString, Asset> m_scanMissingAssets;
};

#endif
