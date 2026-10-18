#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QStringList>
#include "database/DatabaseManager.h"

class TestDatabase : public QObject
{
    Q_OBJECT

    QTemporaryDir *tmpDir = nullptr;
    DatabaseManager *db = nullptr;
    int dbCounter = 0;

    Asset makeAsset(const QString &id, const QString &path) const
    {
        Asset a;
        a.id = id;
        a.filePath = path;
        a.fileName = QFileInfo(path).fileName();
        a.format = QFileInfo(path).suffix();
        a.hash = "hash-" + id;
        a.folderId = QFileInfo(path).absolutePath();
        a.createdAt = QDateTime::currentDateTime();
        a.updatedAt = a.createdAt;
        return a;
    }

private slots:
    void initTestCase()
    {
        tmpDir = new QTemporaryDir();
        QVERIFY2(tmpDir->isValid(), qPrintable(tmpDir->errorString()));
    }

    // Every test gets its own database, so slots can run independently
    // (including `tst_database <single-test>`).
    void init()
    {
        db = new DatabaseManager(tmpDir->filePath(QStringLiteral("test_%1.db").arg(++dbCounter)));
        QVERIFY(db->initialize());
    }

    void cleanup()
    {
        delete db;
        db = nullptr;
    }

    void cleanupTestCase()
    {
        delete tmpDir;
    }

    void testInsertAndGetAsset()
    {
        const Asset a = makeAsset("test-id-1", "C:/test/test.png");
        QVERIFY(db->insertAsset(a));

        const Asset retrieved = db->getAsset("test-id-1");
        QCOMPARE(retrieved.id, a.id);
        QCOMPARE(retrieved.filePath, a.filePath);
        QCOMPARE(retrieved.fileName, a.fileName);
    }

    void testInsertTag()
    {
        Tag t;
        t.name = "portrait";
        t.color = QColor("#ff0000");
        const int id = db->insertTag(t);
        QVERIFY(id >= 0);

        Tag t2;
        t2.name = "landscape";
        t2.color = QColor("#00ff00");
        QVERIFY(db->insertTag(t2) >= 0);

        QCOMPARE(db->getAllTags().size(), 2);
    }

    void testAssetTagRelation()
    {
        const Asset a = makeAsset("asset-tag", "C:/test/asset-tag.png");
        QVERIFY(db->insertAsset(a));
        Tag t;
        t.name = "relation";
        const int tagId = db->insertTag(t);
        QVERIFY(tagId >= 0);
        QVERIFY(db->addTagToAsset(a.id, tagId));

        const auto assetTags = db->getTagsForAsset(a.id);
        QCOMPARE(assetTags.size(), 1);
        QCOMPARE(assetTags.first().id, tagId);
    }

    void testMetadata()
    {
        const Asset a = makeAsset("metadata", "C:/test/metadata.png");
        QVERIFY(db->insertAsset(a));

        Metadata m;
        m.assetId = a.id;
        m.source = "stable-diffusion";
        m.prompt = "a cat";
        m.seed = 42;
        m.steps = 20;
        m.cfgScale = 7.0;
        QVERIFY(db->upsertMetadata(m));

        const Metadata retrieved = db->getMetadata(a.id);
        QCOMPARE(retrieved.prompt, "a cat");
        QCOMPARE(retrieved.seed, 42);
        QCOMPARE(retrieved.cfgScale, 7.0);
    }

    void testSearch()
    {
        QVERIFY(db->insertAsset(makeAsset("search", "C:/test/search-me.png")));
        const auto results = db->searchAssets("search", "", {}, false);
        QCOMPARE(results.size(), 1);
    }

    void testFindByPath()
    {
        const Asset a = makeAsset("find", "C:/test/find-me.png");
        QVERIFY(db->insertAsset(a));
        const Asset found = db->findByPath(a.filePath);
        QCOMPARE(found.id, a.id);
    }

    // Rows with identical sort keys must not move between LIMIT/OFFSET pages.
    void testSearchPaginationStable()
    {
        const QDateTime stamp = QDateTime::fromString("2024-01-01T00:00:00", Qt::ISODate);
        QVERIFY(stamp.isValid());
        Tag pageTag;
        pageTag.name = "pagination";
        pageTag.color = QColor("#123456");
        const int tagId = db->insertTag(pageTag);
        QVERIFY(tagId >= 0);

        for (int i = 0; i < 9; ++i) {
            Asset a = makeAsset(QStringLiteral("page-id-%1").arg(i),
                                QStringLiteral("C:/pagination/page-%1.png").arg(i));
            a.createdAt = stamp;
            a.updatedAt = stamp;
            QVERIFY2(db->insertAsset(a), qPrintable(a.filePath));
            QVERIFY(db->addTagToAsset(a.id, tagId));
        }

        QVector<QString> seen;
        for (int offset = 0; offset < 9; offset += 3) {
            int total = -1;
            const auto page = db->searchAssets("", "", {tagId}, false,
                                               "created_at", true, offset, 3, &total);
            QCOMPARE(page.size(), 3);
            QCOMPARE(total, 9);
            for (const auto &a : page) {
                QVERIFY2(!seen.contains(a.id), qPrintable(QStringLiteral("duplicate row %1").arg(a.id)));
                seen.append(a.id);
            }
        }
        QCOMPARE(seen.size(), 9);
    }

    void testFolderFilterIncludesSubfolders()
    {
        const QString root = "C:/folderfilter";
        const Asset rootAsset = makeAsset("folder-root", root + "/root.png");
        const Asset nestedAsset = makeAsset("folder-nested", root + "/sub/nested.png");
        const Asset otherAsset = makeAsset("folder-other", "C:/folderfilter-other/x.png");
        QVERIFY(db->insertAsset(rootAsset));
        QVERIFY(db->insertAsset(nestedAsset));
        QVERIFY(db->insertAsset(otherAsset));

        int total = -1;
        const auto assets = db->searchAssets("", "", {}, false, "created_at", true, 0, -1,
                                             &total, root);
        QCOMPARE(total, 2);
        QCOMPARE(assets.size(), 2);
        QStringList ids;
        for (const auto &a : assets)
            ids << a.id;
        QVERIFY(ids.contains("folder-root"));
        QVERIFY(ids.contains("folder-nested"));
        QVERIFY(!ids.contains("folder-other"));

        // Native separators are normalized before matching.
        QCOMPARE(db->countAssets("", "", {}, false, "C:\\folderfilter"), 2);
    }

    void testDeleteAsset()
    {
        const Asset a = makeAsset("delete-me", "C:/test/delete-me.png");
        QVERIFY(db->insertAsset(a));
        QVERIFY(db->deleteAsset(a.id));
        QVERIFY(db->getAsset(a.id).id.isEmpty());
    }
};

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"
