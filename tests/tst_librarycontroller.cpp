#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QImage>
#include <QSignalSpy>
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
        m_ctrl->scanAndInsertFile(newPath);

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

        // Rescanning the same folder must not duplicate rows.
        m_scanner->scanDirectory(scanDir, false);
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 2, 5000);
        QCOMPARE(m_db->countAssets(), before + 3);
    }
};

QTEST_MAIN(TestLibraryController)
#include "tst_librarycontroller.moc"
