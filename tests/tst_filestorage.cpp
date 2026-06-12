#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "models/Asset.h"

class TestFileStorage : public QObject
{
    Q_OBJECT

private slots:
    void testComputeHash()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/test.png";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("hello world");
        f.close();

        QString hash = FileScanner::computeHash(path);
        QCOMPARE(hash.size(), 64);
    }

    void testDetectFormat()
    {
        QCOMPARE(FileScanner::detectFormat("test.PNG"), "png");
        QCOMPARE(FileScanner::detectFormat("test.JPG"), "jpg");
        QCOMPARE(FileScanner::detectFormat("test.webp"), "webp");
    }

    void testImageCacheGenerate()
    {
        ImageCache cache(10);

        QTemporaryDir dir;
        QString path = dir.path() + "/test.png";
        QImage img(100, 100, QImage::Format_RGB32);
        img.fill(Qt::red);
        img.save(path);

        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        QVERIFY(!thumb.isNull());
        QCOMPARE(thumb.width(), 50);
        QCOMPARE(thumb.height(), 50);

        cache.insert(path, QSize(50, 50), thumb);
        QPixmap cached = cache.get(path, QSize(50, 50));
        QVERIFY(!cached.isNull());
    }
};

QTEST_MAIN(TestFileStorage)
#include "tst_filestorage.moc"
