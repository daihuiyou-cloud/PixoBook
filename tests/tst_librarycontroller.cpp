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

    void testScanAndInsertFileIsIdempotent()
    {
        const QString path = m_dir->filePath("imported.png");
        QImage img(32, 24, QImage::Format_RGB32);
        img.fill(Qt::cyan);
        QVERIFY(img.save(path));

        const int before = m_db->countAssets();
        m_ctrl->scanAndInsertFile(path);
        m_ctrl->scanAndInsertFile(path);
        QCOMPARE(m_db->findByPath(path).id.isEmpty(), false);
        QCOMPARE(m_db->countAssets(), before + 1);
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
