#ifndef SDPARSER_H
#define SDPARSER_H

#include <QString>
#include "models/Metadata.h"

class SDParser
{
public:
    static Metadata parse(const QString &filePath);
    static Metadata parseFromPNGChunks(const QString &filePath);
};

#endif
