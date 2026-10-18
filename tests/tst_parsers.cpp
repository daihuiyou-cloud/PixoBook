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

    // Builds signature + one tEXt/zTXt chunk + IEND. The parser does not
    // validate CRCs, so a zero CRC keeps the fixture small.
    static QByteArray makePngWithChunk(const QByteArray &type, const QByteArray &payload)
    {
        QByteArray png("\x89PNG\r\n\x1a\n", 8);
        auto appendChunk = [&png](const QByteArray &chunkType, const QByteArray &data) {
            const quint32 len = static_cast<quint32>(data.size());
            png.append(static_cast<char>((len >> 24) & 0xff));
            png.append(static_cast<char>((len >> 16) & 0xff));
            png.append(static_cast<char>((len >> 8) & 0xff));
            png.append(static_cast<char>(len & 0xff));
            png.append(chunkType);
            png.append(data);
            png.append(4, '\0');
        };
        appendChunk(type, payload);
        appendChunk("IEND", {});
        return png;
    }

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

    void testSDParserSourceName()
    {
        SDParser parser;
        QCOMPARE(parser.sourceName(), "stable-diffusion");
    }

    void testSDParserTextChunk()
    {
        const QByteArray payload = QByteArray("parameters\0", 11)
            + "a cat\nSteps: 20, Seed: 42, CFG scale: 7, Sampler: Euler, Model: test-model";

        Metadata meta;
        QVERIFY(SDParser::tryParseFromData(makePngWithChunk("tEXt", payload), meta));
        QCOMPARE(meta.source, QStringLiteral("stable-diffusion"));
        QCOMPARE(meta.prompt, QStringLiteral("a cat"));
        QCOMPARE(meta.steps, 20);
        QCOMPARE(meta.seed, qint64(42));
        QCOMPARE(meta.cfgScale, 7.0);
        QCOMPARE(meta.sampler, QStringLiteral("Euler"));
        QCOMPARE(meta.modelName, QStringLiteral("test-model"));
    }

    void testSDParserCompressedTextChunk()
    {
        const QString value = QStringLiteral(
            "a dog\nSteps: 30, Seed: 7, CFG scale: 5.5, Sampler: DPM++ 2M");
        QByteArray payload("parameters\0", 11);
        payload.append('\0'); // zTXt compression method: deflate
        payload.append(qCompress(value.toUtf8()));

        Metadata meta;
        QVERIFY(SDParser::tryParseFromData(makePngWithChunk("zTXt", payload), meta));
        QCOMPARE(meta.prompt, QStringLiteral("a dog"));
        QCOMPARE(meta.steps, 30);
        QCOMPARE(meta.seed, qint64(7));
        QCOMPARE(meta.cfgScale, 5.5);
        QCOMPARE(meta.sampler, QStringLiteral("DPM++ 2M"));
    }

    void testSDParserRejectsNonPng()
    {
        Metadata meta;
        QVERIFY(!SDParser::tryParseFromData(QByteArray("not a png"), meta));
    }

    void testMJParserSourceName()
    {
        MJParser parser;
        QCOMPARE(parser.sourceName(), "midjourney");
    }

    void testDALLEParserSourceName()
    {
        DALLEParser parser;
        QCOMPARE(parser.sourceName(), "dalle");
    }

    void testMJParserNoSeed()
    {
        MJParser parser;
        Metadata m = parser.parse("C:/fake/my_image.png");
        QCOMPARE(m.source, "midjourney");
        QCOMPARE(m.seed, 0);
    }

    void testMJParserUpscaledFilename()
    {
        MJParser parser;
        Metadata m = parser.parse("C:/fake/User_12345_6789_Upscaled.png");
        QCOMPARE(m.source, "midjourney");
        QCOMPARE(m.seed, 12345);
    }

    void testMJParserInvalidSeed()
    {
        MJParser parser;
        Metadata m = parser.parse("C:/fake/User_abc_6789.png");
        QCOMPARE(m.source, "midjourney");
        QCOMPARE(m.seed, 0);
    }

    void testDALLEParserNoPrompt()
    {
        DALLEParser parser;
        Metadata m = parser.parse("C:/fake/DALL\u00B7E 2024-01-15 12.30.00.png");
        QCOMPARE(m.source, "dalle");
        QVERIFY(m.prompt.isEmpty());
    }


    void testDALLEParserNonDalleFile()
    {
        DALLEParser parser;
        Metadata m = parser.parse("C:/fake/random_image.png");
        QCOMPARE(m.source, "dalle");
        QVERIFY(m.prompt.isEmpty());
    }
};

QTEST_MAIN(TestParsers)
#include "tst_parsers.moc"