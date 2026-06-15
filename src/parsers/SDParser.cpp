#include "SDParser.h"
#include <QFile>
#include <QDebug>

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

    int pos = 8;
    while (pos + 8 <= data.size()) {
        quint32 chunkLen = (static_cast<unsigned char>(data[pos]) << 24) |
                           (static_cast<unsigned char>(data[pos+1]) << 16) |
                           (static_cast<unsigned char>(data[pos+2]) << 8) |
                           static_cast<unsigned char>(data[pos+3]);
        QByteArray chunkType = data.mid(pos + 4, 4);

        if (chunkType == "tEXt" || chunkType == "zTXt") {
            int dataStart = pos + 8;
            int dataEnd = dataStart + chunkLen;
            if (dataEnd > data.size()) break;

            QByteArray chunkData = data.mid(dataStart, chunkLen);

            if (chunkType == "zTXt") {
                int nullPos = chunkData.indexOf('\0');
                if (nullPos < 0) break;
                QByteArray keyword = chunkData.left(nullPos);
                if (nullPos + 2 > chunkData.size()) break;
                QByteArray compressed = chunkData.mid(nullPos + 2);
                chunkData = qUncompress(compressed);
                if (chunkData.isEmpty()) break;
                QByteArray fullData = keyword + '\0' + chunkData;
                chunkData = fullData;
            }

            int nullPos = chunkData.indexOf('\0');
            if (nullPos < 0) { pos += 12 + chunkLen; continue; }

            QString key = QString::fromUtf8(chunkData.left(nullPos));
            QString value = QString::fromUtf8(chunkData.mid(nullPos + 1));

            if (key == "parameters" || key == "prompt") {
                meta.rawJson += value + "\n";

                QStringList lines = value.split('\n');
                if (!lines.isEmpty()) {
                    meta.prompt = lines[0].trimmed();
                }
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

                            if (k == "steps") meta.steps = v.toInt();
                            else if (k == "seed") meta.seed = v.toLongLong();
                            else if (k == "cfg scale") meta.cfgScale = v.toDouble();
                            else if (k == "model") meta.modelName = v;
                            else if (k == "sampler") meta.sampler = v;
                        }
                    }
                }
            }
        }

        pos += 12 + chunkLen;
        if (chunkType == "IEND") break;
    }

    meta.source = "stable-diffusion";
    return meta;
}
