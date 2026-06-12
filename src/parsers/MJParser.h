#ifndef MJPARSER_H
#define MJPARSER_H

#include <QString>
#include "core/IParser.h"
#include "models/Metadata.h"

class MJParser : public IParser
{
public:
    Metadata parse(const QString &filePath) override;
    QString sourceName() const override { return QStringLiteral("midjourney"); }
};

#endif
