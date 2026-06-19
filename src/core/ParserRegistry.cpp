#include "ParserRegistry.h"
#include "core/ImageFormats.h"
#include "parsers/SDParser.h"
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>

ParserRegistry& ParserRegistry::instance()
{
    static ParserRegistry registry;
    return registry;
}

void ParserRegistry::registerParser(std::unique_ptr<IParser> parser)
{
    m_parsers.emplace(parser->sourceName(), std::move(parser));
}

IParser* ParserRegistry::parserForSource(const QString &source) const
{
    auto it = m_parsers.find(source);
    return it != m_parsers.end() ? it->second.get() : nullptr;
}

QStringList ParserRegistry::registeredSources() const
{
    QStringList keys;
    for (const auto &pair : m_parsers)
        keys.append(pair.first);
    return keys;
}

Metadata ParserRegistry::parse(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (!isSupportedImageFormat(ext))
        return {};

    // For PNG files, read data once and share between detection and parsing
    if (ext == QLatin1String("png")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        QByteArray data = file.readAll();
        {
            Metadata sdMeta;
            if (SDParser::tryParseFromData(data, sdMeta))
                return sdMeta;
        }
        // Non-SD PNG: fall through to filename-based detection
        QString name = fi.fileName().toLower();
        if (name.contains("dall") || name.contains(QRegularExpression("\\d{4}-\\d{2}-\\d{2}"))) {
            IParser *parser = parserForSource("dalle");
            if (parser)
                return parser->parse(filePath);
        }
        if (name.contains("midjourney") || name.contains(QRegularExpression("^[A-Za-z0-9]+_\\d+_"))) {
            IParser *parser = parserForSource("midjourney");
            if (parser)
                return parser->parse(filePath);
        }
        return {};
    }

    QString source = detectSource(filePath);
    IParser *parser = parserForSource(source);
    if (parser)
        return parser->parse(filePath);

    return {};
}

QString ParserRegistry::detectSource(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString name = fi.fileName().toLower();
    QString ext = fi.suffix().toLower();

    if (ext == QLatin1String("png")) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            if (SDParser::containsSdMetadata(data))
                return "stable-diffusion";
        }
    }

    if (name.contains("dall") || name.contains(QRegularExpression("\\d{4}-\\d{2}-\\d{2}")))
        return "dalle";

    if (name.contains("midjourney") || name.contains(QRegularExpression("^[A-Za-z0-9]+_\\d+_")))
        return "midjourney";

    return "stable-diffusion";
}
