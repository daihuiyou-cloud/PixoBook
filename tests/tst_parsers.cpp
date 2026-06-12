#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "parsers/MetadataParser.h"
#include "parsers/SDParser.h"
#include "parsers/MJParser.h"
#include "parsers/DALLEParser.h"

class TestParsers : public QObject
{
    Q_OBJECT

private slots:
    void testMJParser()
    {
        Metadata m = MJParser::parse("C:/fake/test.png", "User_12345_6789.png");
        QCOMPARE(m.source, "midjourney");
        QCOMPARE(m.seed, 12345);
    }

    void testDALLEParser()
    {
        Metadata m = DALLEParser::parse("C:/fake/test.png",
            "DALL·E 2024-01-15 12.30.00 - a cute cat.png");
        QCOMPARE(m.source, "dalle");
        QVERIFY(m.prompt.contains("a cute cat"));
    }

    void testDetectSourceByFilename()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/DALL·E 2024-01-01 test.png";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("fake png data");
        f.close();

        QString source = MetadataParser::detectSource(path);
        QCOMPARE(source, "dalle");
    }

    void testDetectSourceDefault()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/untitled.png";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("fake png data");
        f.close();

        QString source = MetadataParser::detectSource(path);
        QCOMPARE(source, "stable-diffusion");
    }
};

QTEST_MAIN(TestParsers)
#include "tst_parsers.moc"
