#ifndef SDPARSER_H
#define SDPARSER_H

#include <QString>
#include "core/IParser.h"
#include "models/Metadata.h"

class SDParser : public IParser
{
public:
    Metadata parse(const QString &filePath) override;
    QString sourceName() const override { return QStringLiteral("stable-diffusion"); }

    Metadata parseFromPNGChunks(const QString &filePath) const;
};

#endif
