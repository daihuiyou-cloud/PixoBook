#include "DALLEParser.h"
#include <QRegularExpression>
#include <QFileInfo>

Metadata DALLEParser::parse(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString fileName = fi.fileName();
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
