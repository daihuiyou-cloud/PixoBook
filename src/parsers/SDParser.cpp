#include "SDParser.h"
#include <QFile>

Metadata SDParser::parse(const QString &filePath)
{
    return parseFromPNGChunks(filePath);
}

Metadata SDParser::parseFromPNGChunks(const QString &filePath) const
{
    Metadata meta;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return meta;

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 8 || data.left(8) != QByteArray("\x89PNG\r\n\x1a\n", 8))
        return meta;

    const char *raw = data.constData();
    int rawSize = data.size();

    int pos = 8;
    while (pos + 8 <= rawSize) {
        quint32 chunkLen = (static_cast<unsigned char>(raw[pos]) << 24) |
                           (static_cast<unsigned char>(raw[pos+1]) << 16) |
                           (static_cast<unsigned char>(raw[pos+2]) << 8) |
                           static_cast<unsigned char>(raw[pos+3]);

        const char *ct = raw + pos + 4;
        bool isTExt = (ct[0] == 't' && ct[1] == 'E' && ct[2] == 'X' && ct[3] == 't');
        bool isZTXt = (ct[0] == 'z' && ct[1] == 'T' && ct[2] == 'X' && ct[3] == 't');

        if (isTExt || isZTXt) {
            int dataStart = pos + 8;
            int dataEnd = dataStart + chunkLen;
            if (dataEnd > rawSize) break;

            QString key, value;

            if (isZTXt) {
                QByteArray chunkData = data.mid(dataStart, chunkLen);
                int nullPos = chunkData.indexOf('\0');
                if (nullPos < 0) { pos += 12 + chunkLen; continue; }
                QByteArray kw = chunkData.left(nullPos);
                if (nullPos + 2 > chunkData.size()) { pos += 12 + chunkLen; continue; }
                QByteArray compressed = chunkData.mid(nullPos + 2);
                QByteArray decompressed = qUncompress(compressed);
                if (decompressed.isEmpty()) { pos += 12 + chunkLen; continue; }
                key = QString::fromUtf8(kw);
                int vn = decompressed.indexOf('\0');
                value = (vn < 0) ? QString::fromUtf8(decompressed) : QString::fromUtf8(decompressed.constData(), vn);
            } else {
                int nullPos = data.indexOf('\0', dataStart);
                if (nullPos < 0 || nullPos >= dataEnd) { pos += 12 + chunkLen; continue; }
                key = QString::fromUtf8(raw + dataStart, nullPos - dataStart);
                value = QString::fromUtf8(raw + nullPos + 1, dataEnd - nullPos - 1);
            }

            if (key == QLatin1String("parameters") || key == QLatin1String("prompt")) {
                meta.rawJson += value + '\n';

                QStringList lines = value.split('\n');
                if (!lines.isEmpty())
                    meta.prompt = lines[0].trimmed();
                for (int i = 1; i < lines.size(); i++) {
                    QString line = lines[i].trimmed();
                    if (line.startsWith("Negative prompt:", Qt::CaseInsensitive)) {
                        meta.negativePrompt = line.mid(16).trimmed();
                    } else {
                        QStringList parts = line.split(',');
                        for (const auto &part : parts) {
                            auto kv = part.trimmed().split(':');
                            if (kv.size() != 2) continue;
                            QString k = kv[0].trimmed().toLower();
                            QString v = kv[1].trimmed();

                            if (k == QLatin1String("steps")) meta.steps = v.toInt();
                            else if (k == QLatin1String("seed")) meta.seed = v.toLongLong();
                            else if (k == QLatin1String("cfg scale")) meta.cfgScale = v.toDouble();
                            else if (k == QLatin1String("model")) meta.modelName = v;
                            else if (k == QLatin1String("sampler")) meta.sampler = v;
                        }
                    }
                }
            }
        }

        pos += 12 + chunkLen;
        if (ct[0] == 'I' && ct[1] == 'E' && ct[2] == 'N' && ct[3] == 'D') break;
    }

    meta.source = "stable-diffusion";
    return meta;
}
