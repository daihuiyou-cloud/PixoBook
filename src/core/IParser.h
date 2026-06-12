#ifndef IPARSER_H
#define IPARSER_H

#include <QString>
#include "models/Metadata.h"

class IParser
{
public:
    virtual ~IParser() = default;
    virtual Metadata parse(const QString &filePath) = 0;
    virtual QString sourceName() const = 0;
};

#endif
