#include "MetadataParser.h"
#include "core/ParserRegistry.h"

Metadata MetadataParser::parse(const QString &filePath)
{
    return ParserRegistry::instance().parse(filePath);
}
