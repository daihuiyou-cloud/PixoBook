#include "ParserRegistry.h"
#include "core/ImageFormats.h"
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

Metadata ParserRegistry::parse(const QString &filePath)
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

    if (ext == "png") {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            if (data.contains("parameters") || data.contains("Negative prompt:"))
                return "stable-diffusion";
        }
    }

    if (name.contains("dall") || name.contains(QRegularExpression("\\d{4}-\\d{2}-\\d{2}")))
        return "dalle";

    if (name.contains("midjourney") || name.contains(QRegularExpression("^[A-Za-z0-9]+_\\d+_")))
        return "midjourney";

    return "stable-diffusion";
}
