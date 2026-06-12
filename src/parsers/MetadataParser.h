#ifndef METADATAPARSER_H
#define METADATAPARSER_H

#include <QString>
#include "models/Metadata.h"

class MetadataParser
{
public:
    static Metadata parse(const QString &filePath);
    static QString detectSource(const QString &filePath);
};

#endif
