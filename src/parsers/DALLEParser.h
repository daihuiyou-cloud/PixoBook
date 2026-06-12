#ifndef DALLEPARSER_H
#define DALLEPARSER_H

#include <QString>
#include "models/Metadata.h"

class DALLEParser
{
public:
    static Metadata parse(const QString &filePath, const QString &fileName);
};

#endif
