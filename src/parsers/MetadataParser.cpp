#include "MetadataParser.h"
#include "core/ParserRegistry.h"
#include <QFileInfo>

Metadata MetadataParser::parse(const QString &filePath)
{
    return ParserRegistry::instance().parse(filePath);
}
