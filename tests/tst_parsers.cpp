#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "parsers/MetadataParser.h"
#include "parsers/SDParser.h"
#include "parsers/MJParser.h"
#include "parsers/DALLEParser.h"
#include "core/ParserRegistry.h"

class TestParsers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        ParserRegistry::instance().registerParser(std::make_unique<SDParser>());
        ParserRegistry::instance().registerParser(std::make_unique<MJParser>());
        ParserRegistry::instance().registerParser(std::make_unique<DALLEParser>());
    }

    void testMJParser()
    {
        MJParser parser;
        Metadata m = parser.parse("C:/fake/User_12345_6789.png");
        QCOMPARE(m.source, "midjourney");
        QCOMPARE(m.seed, 12345);
    }

    void testDALLEParser()
    {
        DALLEParser parser;
        Metadata m = parser.parse("C:/fake/DALL\u00B7E 2024-01-15 12.30.00 - a cute cat.png");
        QCOMPARE(m.source, "dalle");
        QVERIFY(m.prompt.contains("a cute cat"));
    }

    void testDetectSourceByFilename()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/DALL\u00B7E 2024-01-01 test.png";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("fake png data");
        f.close();

        QString source = ParserRegistry::detectSource(path);
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

        QString source = ParserRegistry::detectSource(path);
        QCOMPARE(source, "stable-diffusion");
    }
};

QTEST_MAIN(TestParsers)
#include "tst_parsers.moc"
