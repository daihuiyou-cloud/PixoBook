#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <memory>
#include "database/DatabaseManager.h"
#include "services/FileScanner.h"
#include "services/FileWatcher.h"
#include "services/ImageCache.h"
#include "services/LibraryController.h"
#include "ui/Codicon.h"
#include "ui/CustomStyle.h"
#include "ui/GalleryWidget.h"
#include "ui/MissingFilesDialog.h"
#include "ui/VisualConstants.h"

// Exposes the protected mouse handler so the test can click grid items without
// depending on window exposure (works reliably with QT_QPA_PLATFORM=offscreen).
class TestableGallery : public GalleryWidget
{
public:
    using GalleryWidget::GalleryWidget;

    void clickAt(const QPoint &pos)
    {
        QMouseEvent event(QEvent::MouseButtonPress, pos, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        mousePressEvent(&event);
    }

    void wheelBy(int angleDelta)
    {
        QWheelEvent event(QPointF(100, 100), QPointF(100, 100), QPoint(),
                          QPoint(0, angleDelta), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        wheelEvent(&event);
    }
};

class TestUi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(new CustomStyle());
        Codicon::init();
        QFont f = qApp->font();
        f.setFamily(QStringLiteral("Microsoft YaHei UI"));
        f.setPointSize(10);
        qApp->setFont(f);
    }

    void testRefreshPreservesSelection()
    {
        ImageCache cache(4 * 1024 * 1024);
        TestableGallery gallery(&cache);
        gallery.resize(800, 600);
        gallery.setThumbnailSize(100);

        QVector<Asset> assets;
        for (int i = 0; i < 10; ++i) {
            Asset a;
            a.id = QStringLiteral("asset-%1").arg(i);
            a.filePath = QStringLiteral("C:/nonexistent/asset-%1.png").arg(i);
            a.fileName = QStringLiteral("asset-%1.png").arg(i);
            assets.append(a);
        }
        gallery.setAssets(assets, assets.size());

        // Thumbnail size 100 -> item 120x160, gap 16, top padding 16. Item 5
        // is row 1, column 0 in an 800px-wide grid.
        // GalleryWidget::kLabelHeight is 40 (private but stable layout).
        const int stride = 100 + Visual::GalleryCardPadding * 2 + 40
            + Visual::GalleryCardGap;
        gallery.clickAt(QPoint(Visual::GalleryTopPadding + 20,
                               Visual::GalleryTopPadding + stride + 20));
        QCOMPARE(gallery.selectedAsset().id, QStringLiteral("asset-5"));

        gallery.refreshAssets(assets, assets.size());
        QCOMPARE(gallery.selectedAsset().id, QStringLiteral("asset-5"));

        QVector<Asset> withoutSelected = assets;
        withoutSelected.removeAt(5);
        gallery.refreshAssets(withoutSelected, withoutSelected.size());
        QVERIFY(gallery.selectedAsset().id.isEmpty());
    }

    // A single wheel notch must move the grid. The old row-snapping logic
    // rounded a 60px step back to the same row, so scrolling appeared dead.
    void testWheelScrollsGrid()
    {
        ImageCache cache(4 * 1024 * 1024);
        TestableGallery gallery(&cache);
        gallery.resize(800, 600);
        gallery.setThumbnailSize(100);

        QVector<Asset> assets;
        for (int i = 0; i < 30; ++i) {
            Asset a;
            a.id = QStringLiteral("scroll-%1").arg(i);
            a.filePath = QStringLiteral("C:/nonexistent/scroll-%1.png").arg(i);
            a.fileName = QStringLiteral("scroll-%1.png").arg(i);
            assets.append(a);
        }
        gallery.setAssets(assets, assets.size());

        // Before scrolling, y=150 is still row 0.
        gallery.clickAt(QPoint(Visual::GalleryTopPadding + 20, 150));
        QCOMPARE(gallery.selectedAsset().id, QStringLiteral("scroll-0"));

        gallery.wheelBy(-120); // one standard notch -> 60px down

        // Row 1 now starts at ~132px, so the same point selects item 5.
        gallery.clickAt(QPoint(Visual::GalleryTopPadding + 20, 150));
        QCOMPARE(gallery.selectedAsset().id, QStringLiteral("scroll-5"));
    }

    // Renders the recovery ledger offscreen and writes reference images into
    // <build>/shots so the visual design can be reviewed without a desktop.
    void testMissingFilesDialogRenders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auto db = std::make_unique<DatabaseManager>(dir.filePath(QStringLiteral("ui.db")));
        QVERIFY(db->initialize());

        struct Seed {
            const char *id;
            const char *name;
            int daysMissing;
            bool favorite;
            int tagCount;
        };
        const Seed seeds[] = {
            {"m-1", "角色设定_月光.png", 12, true, 2},
            {"m-2", "forest_light_04.png", 3, false, 1},
            {"m-3", "霓虹街景_final.webp", 0, false, 0},
        };
        for (const auto &seed : seeds) {
            Asset asset;
            asset.id = QString::fromUtf8(seed.id);
            asset.fileName = QString::fromUtf8(seed.name);
            asset.filePath = QStringLiteral("D:/素材库/2024/%1").arg(asset.fileName);
            asset.format = asset.fileName.endsWith(QStringLiteral("webp"))
                           ? QStringLiteral("webp") : QStringLiteral("png");
            asset.hash = QStringLiteral("hash-%1").arg(asset.id);
            asset.fileSize = 2 * 1024 * 1024;
            asset.width = 1024;
            asset.height = 1024;
            asset.createdAt = QDateTime::currentDateTime().addDays(-30);
            asset.updatedAt = asset.createdAt;
            QVERIFY(db->insertAsset(asset));
            QVERIFY(db->markAssetMissing(
                asset.filePath, QDateTime::currentDateTime().addDays(-seed.daysMissing)));
            if (seed.favorite)
                QVERIFY(db->updateAssetFavorite(asset.id, true));
            for (int t = 0; t < seed.tagCount; ++t) {
                Tag tag;
                tag.name = QStringLiteral("%1-tag-%2").arg(asset.id).arg(t);
                const int tagId = db->insertTag(tag);
                QVERIFY(tagId > 0);
                QVERIFY(db->addTagToAsset(asset.id, tagId));
            }
        }

        ImageCache cache(4 * 1024 * 1024);
        FileScanner scanner;
        FileWatcher watcher;
        LibraryController controller(db.get(), &cache, &scanner, &watcher);

        MissingFilesDialog dialog(&controller);
        dialog.show();
        QTest::qWait(80);
        const QPixmap shot = dialog.grab();
        QVERIFY(!shot.isNull());
        QCOMPARE(shot.size(), dialog.size());

        const QString shotsDir = QCoreApplication::applicationDirPath()
                                 + QStringLiteral("/shots");
        QVERIFY(QDir().mkpath(shotsDir));
        const QString shotPath = shotsDir + QStringLiteral("/missing-files-dark.png");
        QVERIFY2(shot.save(shotPath), qPrintable(shotPath));
        qInfo() << "saved" << shotPath;

        for (const auto &seed : seeds)
            QVERIFY(db->markAssetPresent(QString::fromUtf8(seed.id)));

        MissingFilesDialog empty(&controller);
        empty.show();
        QTest::qWait(80);
        const QPixmap emptyShot = empty.grab();
        QVERIFY(!emptyShot.isNull());
        QVERIFY(emptyShot.save(shotsDir + QStringLiteral("/missing-files-empty.png")));
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
