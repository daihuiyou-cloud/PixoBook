#include "DALLEParser.h"
#include <QRegularExpression>

Metadata DALLEParser::parse(const QString &filePath, const QString &fileName)
{
    Q_UNUSED(filePath)
    Metadata meta;
    meta.source = "dalle";

    static QRegularExpression promptRe(
        "\\d{4}-\\d{2}-\\d{2} \\d{2}\\.\\d{2}\\.\\d{2} - (.+)\\.",
        QRegularExpression::CaseInsensitiveOption);
    auto match = promptRe.match(fileName);
    if (match.hasMatch()) {
        meta.prompt = match.captured(1).trimmed();
    }

    return meta;
}
