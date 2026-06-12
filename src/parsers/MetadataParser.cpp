#include "MetadataParser.h"
#include "SDParser.h"
#include "MJParser.h"
#include "DALLEParser.h"
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>

Metadata MetadataParser::parse(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "webp")
        return {};

    QString source = detectSource(filePath);
    if (source == "stable-diffusion")
        return SDParser::parse(filePath);
    else if (source == "midjourney")
        return MJParser::parse(filePath, fi.fileName());
    else if (source == "dalle")
        return DALLEParser::parse(filePath, fi.fileName());

    return {};
}

QString MetadataParser::detectSource(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString name = fi.fileName().toLower();

    if (fi.suffix().toLower() == "png") {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            if (data.contains("parameters") || data.contains("Negative prompt:"))
                return "stable-diffusion";
        }
    }

    if (name.contains("dall") || name.contains(QRegularExpression("\\d{4}-\\d{2}-\\d{2}")))
        return "dalle";

    if (name.contains("midjourney") || name.contains(QRegularExpression("^[A-Za-z0-9]+_\\d+_")))
        return "midjourney";

    return "stable-diffusion";
}
