#include <QtTest/QtTest>
#include "models/Asset.h"
#include "models/Tag.h"
#include "models/Metadata.h"

class TestModels : public QObject
{
    Q_OBJECT

private slots:
    void testAssetDefaults()
    {
        Asset a;
        QCOMPARE(a.fileSize, 0);
        QCOMPARE(a.width, 0);
        QCOMPARE(a.isFavorite, false);
        QVERIFY(a.id.isEmpty());
    }

    void testTagValidity()
    {
        Tag t;
        QVERIFY(!t.isValid());
        t.id = 1;
        t.name = "portrait";
        QVERIFY(t.isValid());
    }

    void testMetadataDefaults()
    {
        Metadata m;
        QVERIFY(m.assetId.isEmpty());
        QCOMPARE(m.seed, 0);
        QCOMPARE(m.steps, 0);
        QCOMPARE(m.cfgScale, 0.0);
    }
};

QTEST_MAIN(TestModels)
#include "tst_models.moc"
