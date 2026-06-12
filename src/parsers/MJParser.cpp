#include "MJParser.h"
#include <QRegularExpression>

Metadata MJParser::parse(const QString &filePath, const QString &fileName)
{
    Q_UNUSED(filePath)
    Metadata meta;
    meta.source = "midjourney";

    static QRegularExpression seedRe("_(\\d+)_", QRegularExpression::CaseInsensitiveOption);
    auto match = seedRe.match(fileName);
    if (match.hasMatch()) {
        meta.seed = match.captured(1).toLongLong();
    }

    static QRegularExpression jobRe("^([A-Za-z0-9]+)_", QRegularExpression::CaseInsensitiveOption);
    match = jobRe.match(fileName);
    if (match.hasMatch()) {
        meta.modelName = "Midjourney";
    }

    return meta;
}
