#ifndef SDPARSER_H
#define SDPARSER_H

#include <QString>
#include "core/IParser.h"
#include "models/Metadata.h"

class SDParser final : public IParser
{
public:
    Metadata parse(const QString &filePath) override;
    QString sourceName() const override { return QStringLiteral("stable-diffusion"); }

    static bool tryParseFromData(const QByteArray &pngData, Metadata &outMeta);
    static bool containsSdMetadata(const QByteArray &pngData);

private:
    Metadata parseFromPNGChunks(const QString &filePath) const;
};

#endif
