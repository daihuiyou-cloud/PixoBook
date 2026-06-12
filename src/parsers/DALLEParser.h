#ifndef DALLEPARSER_H
#define DALLEPARSER_H

#include <QString>
#include "core/IParser.h"
#include "models/Metadata.h"

class DALLEParser : public IParser
{
public:
    Metadata parse(const QString &filePath) override;
    QString sourceName() const override { return QStringLiteral("dalle"); }
};

#endif
