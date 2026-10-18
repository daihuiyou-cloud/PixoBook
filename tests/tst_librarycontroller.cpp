#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "database/DatabaseManager.h"
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "services/LibraryController.h"
#include "ui/UndoCommands.h"

// Simulates a commit failure (disk full, lock timeout, ...) without a full
// IDatabaseManager mock.
class CommitFailingDatabase : public DatabaseManager
{
public:
    using DatabaseManager::DatabaseManager;
    [[nodiscard]] bool commitTransaction() override { return false; }
};

class TestLibraryController : public QObject
{
    Q_OBJECT

    QTemporaryDir *m_dir = nullptr;
    DatabaseManager *m_db = nullptr;
    ImageCache *m_cache = nullptr;
    FileScanner *m_scanner = nullptr;
    FileWatcher *m_watcher = nullptr;
    LibraryController *m_ctrl = nullptr;
    int m_tagId = -1;

    Asset makeAsset(const QString &id) const
    {
        Asset a;
        a.id = id;
        a.filePath = m_dir->filePath(id + ".png");
        a.fileName = id + ".png";
        a.hash = "hash-" + id;
        a.createdAt = QDateTime::currentDateTime();
        a.updatedAt = a.createdAt;
        return a;
    }

private slots:
    void initTestCase()
    {
        m_dir = new QTemporaryDir();
        QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));
        m_db = new DatabaseManager(m_dir->filePath("library.db"));
        QVERIFY(m_db->initialize());
        m_cache = new ImageCache(8LL * 1024 * 1024);
        m_scanner = new FileScanner();
        m_watcher = new FileWatcher();
        m_ctrl = new LibraryController(m_db, m_cache, m_scanner, m_watcher);

        Tag tag;
        tag.name = "portrait";
        tag.color = QColor("#ff0000");
        m_tagId = m_db->insertTag(tag);
        QVERIFY(m_tagId >= 0);
    }

    void cleanupTestCase()
    {
        delete m_ctrl;
        delete m_watcher;
        delete m_scanner;
        delete m_cache;
        delete m_db;
        delete m_dir;
    }

    // Undo must not remove a tag that already existed before the command.
    void testAddTagUndoKeepsPreexistingTag()
    {
        const Asset asset = makeAsset("pre-tagged");
        QVERIFY(m_db->insertAsset(asset));
        QVERIFY(m_db->addTagToAsset(asset.id, m_tagId));

        AddTagToAssetsCommand command(m_ctrl, {asset.id}, m_tagId);
        command.redo();
        command.undo();

        const auto tags = m_db->getTagsForAsset(asset.id);
        bool stillTagged = false;
        for (const auto &tag : tags) {
            if (tag.id == m_tagId)
                stillTagged = true;
        }
        QVERIFY2(stillTagged, "undo removed a tag that was already assigned");
    }

    void testAddTagUndoRemovesNewTag()
    {
        const Asset asset = makeAsset("newly-tagged");
        QVERIFY(m_db->insertAsset(asset));

        AddTagToAssetsCommand command(m_ctrl, {asset.id}, m_tagId);
        command.redo();
        QCOMPARE(m_db->getTagsForAsset(asset.id).size(), 1);

        command.undo();
        QVERIFY(m_db->getTagsForAsset(asset.id).isEmpty());
    }

    void testDeleteUndoRestoresMetadataAndTags()
    {
        const Asset asset = makeAsset("restore-me");
        QVERIFY(m_db->insertAsset(asset));
        QVERIFY(m_db->addTagToAsset(asset.id, m_tagId));

        Metadata meta;
        meta.assetId = asset.id;
        meta.source = "stable-diffusion";
        meta.prompt = "a restored prompt";
        QVERIFY(m_db->upsertMetadata(meta));

        DeleteAssetsCommand command(m_ctrl, {asset},
                                    {m_db->getMetadata(asset.id)},
                                    {m_db->getTagIdsForAsset(asset.id)});
        command.redo();
        QVERIFY(m_db->getAsset(asset.id).id.isEmpty());

        command.undo();
        QCOMPARE(m_db->getAsset(asset.id).id, asset.id);
        QCOMPARE(m_db->getMetadata(asset.id).prompt, meta.prompt);
        QCOMPARE(m_db->getTagsForAsset(asset.id).size(), 1);
    }

    void testRenameKeepsAssetIdentity()
    {
        const QString oldPath = m_dir->filePath("rename-a.png");
        const QString newPath = m_dir->filePath("rename-b.png");
        QImage img(24, 24, QImage::Format_RGB32);
        img.fill(Qt::darkYellow);
        QVERIFY(img.save(oldPath));
        QVERIFY(m_ctrl->scanAndInsertFile(oldPath));

        const Asset before = m_db->findByPath(oldPath);
        QVERIFY(!before.id.isEmpty());
        QVERIFY(m_db->addTagToAsset(before.id, m_tagId));
        QVERIFY(QFile::rename(oldPath, newPath));

        // Watcher/manual import sees the new path as a duplicate hash; it must
        // adopt the existing row instead of creating a second one (and before
        // the subsequent fileRemoved event deletes the old row).
        (void)m_ctrl->scanAndInsertFile(newPath);

        const Asset after = m_db->findByPath(newPath);
        QCOMPARE(after.id, before.id);
        QVERIFY(m_db->findByPath(oldPath).id.isEmpty());
        QCOMPARE(m_db->getTagsForAsset(after.id).size(), 1);
    }

    void testDeleteAssetsCommitFailureReturnsFalse()
    {
        QTemporaryDir dbDir;
        CommitFailingDatabase failingDb(dbDir.filePath("failing.db"));
        QVERIFY(failingDb.initialize());
        const Asset asset = makeAsset("commit-fail");
        QVERIFY(failingDb.insertAsset(asset));

        ImageCache cache(1024 * 1024);
        FileScanner scanner;
        FileWatcher watcher;
        LibraryController controller(&failingDb, &cache, &scanner, &watcher);

        QVERIFY(!controller.deleteAssets({asset}));
        // The failed commit must roll back the delete.
        QVERIFY(!failingDb.getAsset(asset.id).id.isEmpty());
    }

    void testScanAndInsertFileIsIdempotent()
    {
        const QString path = m_dir->filePath("imported.png");
        QImage img(32, 24, QImage::Format_RGB32);
        img.fill(Qt::cyan);
        QVERIFY(img.save(path));

        const int before = m_db->countAssets();
        QVERIFY(m_ctrl->scanAndInsertFile(path));
        QVERIFY(!m_ctrl->scanAndInsertFile(path));
        QCOMPARE(m_db->findByPath(path).id.isEmpty(), false);
        QCOMPARE(m_db->countAssets(), before + 1);
    }

    void testScanDirectorySkipsKnownPaths()
    {
        const QString scanDir = m_dir->filePath("known");
        QVERIFY(QDir().mkpath(scanDir));
        QStringList paths;
        for (int i = 0; i < 3; ++i) {
            QImage img(20, 20, QImage::Format_RGB32);
            img.fill(QColor(0, 0, 40 + i * 40));
            paths << scanDir + QStringLiteral("/known-%1.png").arg(i);
            QVERIFY(img.save(paths.last()));
        }

        const int before = m_db->countAssets();
        QSignalSpy finishedSpy(m_ctrl, &LibraryController::scanFinished);
        m_scanner->scanDirectory(scanDir, false, QSet<QString>{paths.first()});
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 5000);

        // The pre-known path is filtered before any hash/metadata work and must
        // not reach the database; the other two are imported normally.
        QCOMPARE(m_db->countAssets(), before + 2);
        QVERIFY(m_db->findByPath(paths.first()).id.isEmpty());
        QVERIFY(!m_db->findByPath(paths[1]).id.isEmpty());
        QVERIFY(!m_db->findByPath(paths[2]).id.isEmpty());
    }

    void testWatcherImportsFilesInNewSubdirectory()
    {
        const QString root = m_dir->filePath("watch-root");
        QVERIFY(QDir().mkpath(root));
        m_watcher->addWatchPath(root);

        const int before = m_db->countAssets();
        QSignalSpy addedSpy(m_watcher, &FileWatcher::fileAdded);
        const QString subDir = root + "/new-sub";
        QVERIFY(QDir().mkpath(subDir));
        QImage img(16, 16, QImage::Format_RGB32);
        img.fill(Qt::magenta);
        QVERIFY(img.save(subDir + "/inside.png"));

        QTRY_VERIFY_WITH_TIMEOUT(addedSpy.count() >= 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(m_db->countAssets(), before + 1, 5000);
        m_watcher->removeWatchPath(root);
    }

    // A removal event is a state change, not a delete: the row, its tags and
    // favorite must survive so the user can restore the file later.
    void testWatcherRemovalMarksMissingInsteadOfDeleting()
    {
        const QString path = m_dir->filePath("vanishing.png");
        QImage img(24, 24, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        QVERIFY(img.save(path));
        QVERIFY(m_ctrl->scanAndInsertFile(path));

        const Asset before = m_db->findByPath(path);
        QVERIFY(!before.id.isEmpty());
        QVERIFY(m_db->addTagToAsset(before.id, m_tagId));

        QMetaObject::invokeMethod(m_watcher, "fileRemoved", Qt::DirectConnection,
                                  Q_ARG(QString, path));

        const Asset marked = m_db->findByPath(path);
        QCOMPARE(marked.id, before.id);
        QVERIFY(marked.isMissing());
        QCOMPARE(m_db->getTagsForAsset(before.id).size(), 1);

        // The file comes back: the same row is restored, not duplicated.
        QVERIFY(!m_ctrl->scanAndInsertFile(path));
        QVERIFY(!m_db->findByPath(path).isMissing());
        QCOMPARE(m_db->findByPath(path).id, before.id);
        QCOMPARE(m_db->getTagsForAsset(before.id).size(), 1);
    }

    // A folder rescan must revisit missing paths instead of skipping them as
    // "already imported".
    void testScanFolderRestoresReturningFile()
    {
        const QString dirPath = m_dir->filePath("restore-scan");
        QVERIFY(QDir().mkpath(dirPath));
        const QString path = dirPath + "/returns.png";
        QImage img(20, 20, QImage::Format_RGB32);
        img.fill(Qt::darkMagenta);
        QVERIFY(img.save(path));

        QSignalSpy finished(m_ctrl, &LibraryController::scanFinished);
        m_ctrl->scanFolder(dirPath);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, 5000);
        const Asset before = m_db->findByPath(path);
        QVERIFY(!before.id.isEmpty());
        QVERIFY(m_db->addTagToAsset(before.id, m_tagId));

        QVERIFY(QFile::remove(path));
        QMetaObject::invokeMethod(m_watcher, "fileRemoved", Qt::DirectConnection,
                                  Q_ARG(QString, path));
        QVERIFY(m_db->findByPath(path).isMissing());

        QVERIFY(img.save(path));
        m_ctrl->scanFolder(dirPath);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 2, 5000);

        QTRY_VERIFY_WITH_TIMEOUT(!m_db->findByPath(path).isMissing(), 5000);
        QCOMPARE(m_db->findByPath(path).id, before.id);
        QCOMPARE(m_db->getTagsForAsset(before.id).size(), 1);
    }

    // Files deleted while the app was closed never reach the watcher; the
    // reconcile pass stats watched folders and marks them missing.
    void testReconcileMarksFilesDeletedWhileClosed()
    {
        const QString root = m_dir->filePath("reconcile-root");
        QVERIFY(QDir().mkpath(root));
        const QString keepPath = root + "/keep.png";
        const QString gonePath = root + "/gone.png";
        QImage img(16, 16, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(keepPath));
        img.fill(Qt::blue);
        QVERIFY(img.save(gonePath));

        QSignalSpy finished(m_ctrl, &LibraryController::scanFinished);
        m_ctrl->scanFolder(root);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, 5000);

        const Asset gone = m_db->findByPath(gonePath);
        QVERIFY(!gone.id.isEmpty());
        QVERIFY(m_db->addTagToAsset(gone.id, m_tagId));

        // Let the automatic post-scan reconcile (1s debounce) finish first.
        QSignalSpy checked(m_ctrl, &LibraryController::missingCheckFinished);
        QTRY_VERIFY_WITH_TIMEOUT(checked.count() >= 1, 5000);
        QVERIFY(!m_db->findByPath(gonePath).isMissing());

        QVERIFY(QFile::remove(gonePath));
        m_ctrl->reconcileMissingFiles();
        QTRY_VERIFY_WITH_TIMEOUT(m_db->findByPath(gonePath).isMissing(), 5000);
        QVERIFY(!m_db->findByPath(keepPath).isMissing());
        QCOMPARE(m_db->getTagsForAsset(gone.id).size(), 1);
    }

    void testRelinkMissingAssetKeepsAnnotations()
    {
        Asset old = makeAsset("relink-old");
        QVERIFY(m_db->insertAsset(old));
        QVERIFY(m_db->addTagToAsset(old.id, m_tagId));
        QVERIFY(m_db->markAssetMissing(old.filePath, QDateTime::currentDateTime()));
        QVERIFY(m_db->getAsset(old.id).isMissing());

        const QString newPath = m_dir->filePath("relink-new.png");
        QImage img(20, 20, QImage::Format_RGB32);
        img.fill(Qt::yellow);
        QVERIFY(img.save(newPath));

        QString error;
        QVERIFY2(m_ctrl->relinkAsset(old.id, newPath, &error), qPrintable(error));

        const Asset updated = m_db->getAsset(old.id);
        QCOMPARE(updated.filePath, QFileInfo(newPath).absoluteFilePath());
        QVERIFY(!updated.isMissing());
        QVERIFY(!updated.hash.isEmpty());
        QCOMPARE(m_db->findByPath(updated.filePath).id, old.id);
        QCOMPARE(m_db->getTagsForAsset(old.id).size(), 1);
    }

    void testImportAnnotationsMergesByHash()
    {
        Asset target = makeAsset("import-target");
        QVERIFY(m_db->insertAsset(target));

        QJsonObject entry;
        entry[QStringLiteral("id")] = QStringLiteral("exported-id");
        entry[QStringLiteral("filePath")] = target.filePath;
        entry[QStringLiteral("hash")] = target.hash;
        entry[QStringLiteral("isFavorite")] = true;

        QJsonObject tag;
        tag[QStringLiteral("name")] = QStringLiteral("imported-tag");
        tag[QStringLiteral("color")] = QStringLiteral("#ff8800");
        entry[QStringLiteral("tags")] = QJsonArray{tag};

        QJsonObject meta;
        meta[QStringLiteral("source")] = QStringLiteral("stable-diffusion");
        meta[QStringLiteral("prompt")] = QStringLiteral("imported prompt");
        meta[QStringLiteral("seed")] = 12345;
        entry[QStringLiteral("metadata")] = meta;

        QJsonObject unknown;
        unknown[QStringLiteral("filePath")] = m_dir->filePath("not-here.png");
        unknown[QStringLiteral("hash")] = QStringLiteral("hash-does-not-exist");

        QJsonObject root;
        root[QStringLiteral("format")] = QStringLiteral("pixobook-annotations");
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("assets")] = QJsonArray{entry, unknown};

        const QString jsonPath = m_dir->filePath("import-annotations.json");
        QFile file(jsonPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(root).toJson());
        file.close();

        LibraryController::ImportSummary summary;
        QString error;
        QVERIFY2(m_ctrl->importAnnotations(jsonPath, &summary, &error), qPrintable(error));
        QCOMPARE(summary.matched, 1);
        QCOMPARE(summary.unmatched, 1);
        QCOMPARE(summary.tagsCreated, 1);
        QCOMPARE(summary.favoritesSet, 1);
        QCOMPARE(summary.metadataFilled, 1);

        QVERIFY(m_db->getAsset(target.id).isFavorite);
        QCOMPARE(m_db->getTagsForAsset(target.id).size(), 1);
        QCOMPARE(m_db->getTagsForAsset(target.id).first().name,
                 QStringLiteral("imported-tag"));
        QCOMPARE(m_db->getMetadata(target.id).prompt, QStringLiteral("imported prompt"));
        QCOMPARE(m_db->getMetadata(target.id).seed, 12345LL);
    }

    void testExportAnnotationsWritesTagsAndPrompt()
    {
        const QString path = m_dir->filePath("export-me.png");
        QImage img(18, 18, QImage::Format_RGB32);
        img.fill(Qt::darkCyan);
        QVERIFY(img.save(path));
        QVERIFY(m_ctrl->scanAndInsertFile(path));
        const Asset asset = m_db->findByPath(path);
        QVERIFY(!asset.id.isEmpty());
        QVERIFY(m_db->addTagToAsset(asset.id, m_tagId));
        QVERIFY(m_db->updateAssetFavorite(asset.id, true));

        Metadata meta;
        meta.assetId = asset.id;
        meta.source = "stable-diffusion";
        meta.prompt = "a lighthouse at dusk";
        meta.seed = 123456;
        QVERIFY(m_db->upsertMetadata(meta));

        const QString jsonPath = m_dir->filePath("annotations.json");
        QString error;
        QVERIFY2(m_ctrl->exportAnnotations(jsonPath, &error), qPrintable(error));

        QFile file(jsonPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        const QJsonObject root = doc.object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(),
                 QStringLiteral("pixobook-annotations"));
        const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
        // The fixture database is shared by all slots, so locate our row
        // instead of assuming the export contains a single asset.
        QJsonObject entry;
        for (const auto &value : assets) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("id")).toString() == asset.id) {
                entry = candidate;
                break;
            }
        }
        QVERIFY2(!entry.isEmpty(), "exported annotation for the test asset is missing");
        QVERIFY(entry.value(QStringLiteral("isFavorite")).toBool());
        const QJsonArray tags = entry.value(QStringLiteral("tags")).toArray();
        QCOMPARE(tags.size(), 1);
        QCOMPARE(tags.first().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("portrait"));
        QCOMPARE(entry.value(QStringLiteral("metadata")).toObject()
                     .value(QStringLiteral("prompt")).toString(),
                 QStringLiteral("a lighthouse at dusk"));
    }

    void testScanDirectoryImportsEachFileOnce()
    {
        const QString scanDir = m_dir->filePath("scan");
        QVERIFY(QDir().mkpath(scanDir));
        for (int i = 0; i < 3; ++i) {
            // Distinct pixel content: identical images are de-duplicated by hash.
            QImage img(20, 20, QImage::Format_RGB32);
            img.fill(QColor(0, 40 + i * 40, 0));
            QVERIFY(img.save(scanDir + QStringLiteral("/item-%1.png").arg(i)));
        }

        const int before = m_db->countAssets();
        QSignalSpy finishedSpy(m_ctrl, &LibraryController::scanFinished);
        m_scanner->scanDirectory(scanDir, false);
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 5000);
        QCOMPARE(m_db->countAssets(), before + 3);
        // Indexing is deferred during the scan and flushed on scanFinished.
        if (m_db->isFullTextSearchEnabled())
            QCOMPARE(m_db->searchAssets(QStringLiteral("item-0"), QString(), {}, false).size(), 1);

        // Rescanning the same folder must not duplicate rows.
        m_scanner->scanDirectory(scanDir, false);
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 2, 5000);
        QCOMPARE(m_db->countAssets(), before + 3);
    }
};

QTEST_MAIN(TestLibraryController)
#include "tst_librarycontroller.moc"
