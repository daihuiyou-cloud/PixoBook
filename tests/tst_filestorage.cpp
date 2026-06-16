#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QImage>
#include <QDir>
#include "services/ImageCache.h"
#include "services/FileScanner.h"
#include "models/Asset.h"

class TestFileStorage : public QObject
{
    Q_OBJECT

    QTemporaryDir *m_dir = nullptr;

    QString filePath(const QString &name) const {
        return m_dir->filePath(name);
    }

    QString createTestImage(const QString &name, int w = 100, int h = 100) {
        QString path = filePath(name);
        QImage img(w, h, QImage::Format_RGB32);
        img.fill(Qt::blue);
        img.save(path);
        return path;
    }

private slots:
    void initTestCase() {
        m_dir = new QTemporaryDir();
    }

    void cleanupTestCase() {
        delete m_dir;
    }

    void testComputeHash()
    {
        QString path = filePath("hash_test.png");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello world");
        f.close();

        QString hash = FileScanner::computeHash(path);
        QCOMPARE(hash.size(), 64);

        QString hash2 = FileScanner::computeHash(path);
        QCOMPARE(hash, hash2);
    }

    void testComputeHashMissingFile()
    {
        QString hash = FileScanner::computeHash("C:/nonexistent/file.png");
        QVERIFY(hash.isEmpty());
    }

    void testComputeHashEmptyFile()
    {
        QString path = filePath("empty.png");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QString hash = FileScanner::computeHash(path);
        QCOMPARE(hash.size(), 64);
    }

    void testDetectFormat()
    {
        QCOMPARE(FileScanner::detectFormat("test.PNG"), "png");
        QCOMPARE(FileScanner::detectFormat("test.JPG"), "jpg");
        QCOMPARE(FileScanner::detectFormat("test.jpeg"), "jpeg");
        QCOMPARE(FileScanner::detectFormat("test.webp"), "webp");
        QCOMPARE(FileScanner::detectFormat("test"), "");
        QCOMPARE(FileScanner::detectFormat("test."), "");
    }

    void testIsSupportedFormat()
    {
        QVERIFY(FileScanner::isSupportedFormat("test.png"));
        QVERIFY(FileScanner::isSupportedFormat("test.jpg"));
        QVERIFY(FileScanner::isSupportedFormat("test.jpeg"));
        QVERIFY(FileScanner::isSupportedFormat("test.webp"));
        QVERIFY(!FileScanner::isSupportedFormat("test.bmp"));
        QVERIFY(!FileScanner::isSupportedFormat("test.gif"));
        QVERIFY(!FileScanner::isSupportedFormat("test.txt"));
        QVERIFY(!FileScanner::isSupportedFormat("test"));
    }

    void testScanSingleFile()
    {
        QString path = createTestImage("single.png", 64, 128);
        Asset a = FileScanner::scanSingleFile(path);
        QVERIFY(!a.id.isEmpty());
        QCOMPARE(a.fileName, "single.png");
        QCOMPARE(a.width, 64);
        QCOMPARE(a.height, 128);
        QCOMPARE(a.format, "png");
        QVERIFY(!a.hash.isEmpty());
        QCOMPARE(a.filePath, QDir::toNativeSeparators(path));
    }

    void testScanSingleFileMissing()
    {
        Asset a = FileScanner::scanSingleFile("C:/nonexistent/missing.png");
        QVERIFY(a.id.isEmpty());
    }

    void testScanSingleFileUnsupportedFormat()
    {
        QString path = filePath("test.txt");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not an image");
        f.close();

        Asset a = FileScanner::scanSingleFile(path);
        QVERIFY(a.id.isEmpty());
    }

    void testScanDirectoryViaScanSingle()
    {
        createTestImage("dir_img1.png", 50, 50);
        createTestImage("dir_img2.jpg", 100, 200);

        Asset a1 = FileScanner::scanSingleFile(filePath("dir_img1.png"));
        QVERIFY(!a1.id.isEmpty());
        QCOMPARE(a1.width, 50);
        QCOMPARE(a1.height, 50);

        Asset a2 = FileScanner::scanSingleFile(filePath("dir_img2.jpg"));
        QVERIFY(!a2.id.isEmpty());
        QCOMPARE(a2.width, 100);
        QCOMPARE(a2.height, 200);
    }

    void testImageCacheGenerate()
    {
        ImageCache cache(10);
        QString path = createTestImage("cache_gen.png", 100, 100);

        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        QVERIFY(!thumb.isNull());
        QCOMPARE(thumb.width(), 50);
        QCOMPARE(thumb.height(), 50);

        cache.insert(path, QSize(50, 50), thumb);
        QPixmap cached = cache.get(path, QSize(50, 50));
        QVERIFY(!cached.isNull());
    }

    void testImageCacheGenerateMissingFile()
    {
        ImageCache cache(10);
        QPixmap thumb = cache.generateThumbnail("C:/nonexistent/missing.png", QSize(50, 50));
        QVERIFY(thumb.isNull());
    }

    void testImageCacheGetNonExistent()
    {
        ImageCache cache(10);
        QPixmap p = cache.get("nonexistent.png", QSize(50, 50));
        QVERIFY(p.isNull());
    }

    void testImageCacheEviction()
    {
        ImageCache cache(1);
        QString path1 = createTestImage("evict1.png", 200, 200);
        QString path2 = createTestImage("evict2.png", 200, 200);

        QPixmap thumb1 = cache.generateThumbnail(path1, QSize(200, 200));
        QVERIFY(!thumb1.isNull());
        cache.insert(path1, QSize(200, 200), thumb1);

        QPixmap thumb2 = cache.generateThumbnail(path2, QSize(200, 200));
        QVERIFY(!thumb2.isNull());
        cache.insert(path2, QSize(200, 200), thumb2);

        QPixmap cached1 = cache.get(path1, QSize(200, 200));
        QPixmap cached2 = cache.get(path2, QSize(200, 200));
        QVERIFY(cached2.isNull() || cached1.isNull());
    }

    void testImageCacheClear()
    {
        ImageCache cache(50);
        QString path = createTestImage("clear.png", 50, 50);
        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        cache.insert(path, QSize(50, 50), thumb);
        QVERIFY(!cache.get(path, QSize(50, 50)).isNull());

        cache.clear();
        QVERIFY(cache.get(path, QSize(50, 50)).isNull());
    }

    void testImageCacheInvalidate()
    {
        ImageCache cache(50);
        QString path = createTestImage("inval.png", 50, 50);
        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        cache.insert(path, QSize(50, 50), thumb);
        QVERIFY(!cache.get(path, QSize(50, 50)).isNull());

        cache.invalidate(path);
        QVERIFY(cache.get(path, QSize(50, 50)).isNull());
    }

    void testImageCacheInvalidateDir()
    {
        ImageCache cache(50);
        QString path = createTestImage("dir_inval.png", 50, 50);
        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        cache.insert(path, QSize(50, 50), thumb);
        QVERIFY(!cache.get(path, QSize(50, 50)).isNull());

        cache.invalidateDir(m_dir->path());
        QVERIFY(cache.get(path, QSize(50, 50)).isNull());
    }

    void testImageCacheDimensionCache()
    {
        ImageCache cache(50);
        QString path = createTestImage("dimcache.png", 80, 120);

        QPixmap thumb1 = cache.generateThumbnail(path, QSize(40, 40));
        QVERIFY(!thumb1.isNull());

        QPixmap thumb2 = cache.generateThumbnail(path, QSize(20, 20));
        QVERIFY(!thumb2.isNull());
    }

    void testImageCacheInsertUpdate()
    {
        ImageCache cache(50);
        QString path = createTestImage("update.png", 50, 50);
        QPixmap thumb = cache.generateThumbnail(path, QSize(50, 50));
        cache.insert(path, QSize(50, 50), thumb);
        QVERIFY(!cache.get(path, QSize(50, 50)).isNull());

        cache.insert(path, QSize(50, 50), thumb);
        QVERIFY(!cache.get(path, QSize(50, 50)).isNull());
    }
};

QTEST_MAIN(TestFileStorage)
#include "tst_filestorage.moc"