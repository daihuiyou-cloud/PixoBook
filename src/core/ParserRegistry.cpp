#include "ParserRegistry.h"
#include "core/ImageFormats.h"
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>

static bool pngContainsSdMetadata(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray sig = file.read(8);
    if (sig.size() < 8 || sig != QByteArray("\x89PNG\r\n\x1a\n", 8))
        return false;

    while (file.bytesAvailable() >= 8) {
        QByteArray lenBytes = file.read(4);
        if (lenBytes.size() < 4) break;
        quint32 chunkLen = (static_cast<unsigned char>(lenBytes[0]) << 24) |
                           (static_cast<unsigned char>(lenBytes[1]) << 16) |
                           (static_cast<unsigned char>(lenBytes[2]) << 8) |
                           static_cast<unsigned char>(lenBytes[3]);
        QByteArray chunkType = file.read(4);
        if (chunkType.size() < 4) break;

        if (chunkType == "tEXt" || chunkType == "zTXt") {
            QByteArray chunkData = file.read(chunkLen);
            if (chunkData.size() < static_cast<int>(chunkLen)) break;
            if (chunkData.contains("parameters") || chunkData.contains("Negative prompt:"))
                return true;
        } else if (chunkLen > 0) {
            file.read(chunkLen);
        }

        file.read(4); // CRC
        if (chunkType == "IEND") break;
    }
    return false;
}

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

    if (ext == QLatin1String("png") && pngContainsSdMetadata(filePath))
        return "stable-diffusion";

    if (name.contains("dall") || name.contains(QRegularExpression("\\d{4}-\\d{2}-\\d{2}")))
        return "dalle";

    if (name.contains("midjourney") || name.contains(QRegularExpression("^[A-Za-z0-9]+_\\d+_")))
        return "midjourney";

    return "stable-diffusion";
}
