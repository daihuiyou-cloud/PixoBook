#include <QtTest/QtTest>
#include <QMouseEvent>
#include "services/ImageCache.h"
#include "ui/GalleryWidget.h"
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
};

class TestUi : public QObject
{
    Q_OBJECT

private slots:
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
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
