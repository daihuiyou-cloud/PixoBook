#ifndef MJPARSER_H
#define MJPARSER_H

#include <QString>
#include "models/Metadata.h"

class MJParser
{
public:
    static Metadata parse(const QString &filePath, const QString &fileName);
};

#endif
