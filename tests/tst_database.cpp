#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "database/DatabaseManager.h"

class TestDatabase : public QObject
{
    Q_OBJECT

    DatabaseManager *db = nullptr;
    QTemporaryDir *tmpDir = nullptr;

private slots:
    void initTestCase()
    {
        tmpDir = new QTemporaryDir();
        db = new DatabaseManager(tmpDir->filePath("test.db"));
        QVERIFY(db->initialize());
    }

    void cleanupTestCase()
    {
        delete db;
        delete tmpDir;
    }

    void testInsertAndGetAsset()
    {
        Asset a;
        a.id = "test-id-1";
        a.filePath = "C:/test/test.png";
        a.fileName = "test.png";
        a.fileSize = 1024;
        a.width = 512;
        a.height = 512;
        a.format = "png";
        a.hash = "abc123";
        a.folderId = "C:/test";
        a.createdAt = QDateTime::currentDateTime();
        a.updatedAt = a.createdAt;
        QVERIFY(db->insertAsset(a));

        Asset retrieved = db->getAsset("test-id-1");
        QCOMPARE(retrieved.id, a.id);
        QCOMPARE(retrieved.filePath, a.filePath);
        QCOMPARE(retrieved.width, 512);
    }

    void testInsertTag()
    {
        Tag t;
        t.name = "portrait";
        t.color = QColor("#ff0000");
        int id = db->insertTag(t);
        QVERIFY(id >= 0);

        Tag t2;
        t2.name = "landscape";
        t2.color = QColor("#00ff00");
        int id2 = db->insertTag(t2);
        QVERIFY(id2 >= 0);

        auto tags = db->getAllTags();
        QVERIFY(tags.size() >= 2);
    }

    void testAssetTagRelation()
    {
        auto tags = db->getAllTags();
        QVERIFY(!tags.isEmpty());
        QVERIFY(db->addTagToAsset("test-id-1", tags[0].id));

        auto assetTags = db->getTagsForAsset("test-id-1");
        bool found = false;
        for (const auto &t : assetTags) {
            if (t.id == tags[0].id) found = true;
        }
        QVERIFY(found);
    }

    void testMetadata()
    {
        Metadata m;
        m.assetId = "test-id-1";
        m.source = "stable-diffusion";
        m.prompt = "a cat";
        m.seed = 42;
        m.steps = 20;
        m.cfgScale = 7.0;
        QVERIFY(db->upsertMetadata(m));

        Metadata retrieved = db->getMetadata("test-id-1");
        QCOMPARE(retrieved.prompt, "a cat");
        QCOMPARE(retrieved.seed, 42);
        QCOMPARE(retrieved.cfgScale, 7.0);
    }

    void testSearch()
    {
        auto results = db->searchAssets("test", "", {}, false);
        QVERIFY(!results.isEmpty());
    }

    void testFindByPath()
    {
        Asset a = db->findByPath("C:/test/test.png");
        QCOMPARE(a.fileName, "test.png");
    }

    void testDeleteAsset()
    {
        QVERIFY(db->deleteAsset("test-id-1"));
        Asset a = db->getAsset("test-id-1");
        QVERIFY(a.id.isEmpty());
    }
};

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"
