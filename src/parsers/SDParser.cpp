#include "SDParser.h"
#include <QFile>

Metadata SDParser::parse(const QString &filePath)
{
    return parseFromPNGChunks(filePath);
}

bool SDParser::containsSdMetadata(const QByteArray &data)
{
    if (data.size() < 8 || data.left(8) != QByteArray("\x89PNG\r\n\x1a\n", 8))
        return false;

    const char *raw = data.constData();
    int rawSize = data.size();
    int pos = 8;

    while (pos + 8 <= rawSize) {
        quint32 chunkLen = (static_cast<unsigned char>(raw[pos]) << 24) |
                           (static_cast<unsigned char>(raw[pos+1]) << 16) |
                           (static_cast<unsigned char>(raw[pos+2]) << 8) |
                           static_cast<unsigned char>(raw[pos+3]);

        if (chunkLen > static_cast<quint32>(rawSize - pos - 12))
            break;

        const char *ct = raw + pos + 4;
        bool isTExt = (ct[0] == 't' && ct[1] == 'E' && ct[2] == 'X' && ct[3] == 't');
        bool isZTXt = (ct[0] == 'z' && ct[1] == 'T' && ct[2] == 'X' && ct[3] == 't');
        bool isITXt = (ct[0] == 'i' && ct[1] == 'T' && ct[2] == 'X' && ct[3] == 't');

        if (isTExt || isZTXt || isITXt) {
            int dataStart = pos + 8;
            int dataEnd = dataStart + chunkLen;
            if (dataEnd > rawSize) break;
            // The keyword is the first NUL-terminated field for all three
            // chunk types (iTXt then adds compression/language fields).
            int nullPos = data.indexOf('\0', dataStart);
            if (nullPos < 0 || nullPos >= dataEnd) {
                pos += 12 + static_cast<int>(chunkLen);
                continue;
            }
            QString key = QString::fromUtf8(raw + dataStart, nullPos - dataStart);
            if (key == QLatin1String("parameters") || key == QLatin1String("prompt"))
                return true;
        }

        pos += 12 + static_cast<int>(chunkLen);
        if (pos > rawSize) break;
        if (ct[0] == 'I' && ct[1] == 'E' && ct[2] == 'N' && ct[3] == 'D') break;
    }
    return false;
}

bool SDParser::tryParseFromData(const QByteArray &data, Metadata &outMeta)
{
    if (data.size() < 8 || data.left(8) != QByteArray("\x89PNG\r\n\x1a\n", 8))
        return false;

    const char *raw = data.constData();
    int rawSize = data.size();
    bool found = false;
    int pos = 8;

    while (pos + 8 <= rawSize) {
        quint32 chunkLen = (static_cast<unsigned char>(raw[pos]) << 24) |
                           (static_cast<unsigned char>(raw[pos+1]) << 16) |
                           (static_cast<unsigned char>(raw[pos+2]) << 8) |
                           static_cast<unsigned char>(raw[pos+3]);

        if (chunkLen > static_cast<quint32>(rawSize - pos - 12))
            break;

        const char *ct = raw + pos + 4;
        bool isTExt = (ct[0] == 't' && ct[1] == 'E' && ct[2] == 'X' && ct[3] == 't');
        bool isZTXt = (ct[0] == 'z' && ct[1] == 'T' && ct[2] == 'X' && ct[3] == 't');
        bool isITXt = (ct[0] == 'i' && ct[1] == 'T' && ct[2] == 'X' && ct[3] == 't');

        if (isTExt || isZTXt || isITXt) {
            int dataStart = pos + 8;
            int dataEnd = dataStart + static_cast<int>(chunkLen);
            if (dataEnd > rawSize) break;

            QString key, value;

            if (isITXt) {
                // keyword NUL, compression flag/method, language NUL,
                // translated keyword NUL, then UTF-8 text (zlib if flag=1).
                QByteArray chunkData = data.mid(dataStart, static_cast<int>(chunkLen));
                const int kwEnd = chunkData.indexOf('\0');
                if (kwEnd < 0 || kwEnd + 3 > chunkData.size()) {
                    pos += 12 + static_cast<int>(chunkLen);
                    continue;
                }
                key = QString::fromUtf8(chunkData.left(kwEnd));
                const int compressionFlag =
                    static_cast<unsigned char>(chunkData.at(kwEnd + 1));
                const int compressionMethod =
                    static_cast<unsigned char>(chunkData.at(kwEnd + 2));
                int cursor = kwEnd + 3;
                const int langEnd = chunkData.indexOf('\0', cursor);
                if (langEnd < 0) {
                    pos += 12 + static_cast<int>(chunkLen);
                    continue;
                }
                cursor = langEnd + 1;
                const int translatedEnd = chunkData.indexOf('\0', cursor);
                if (translatedEnd < 0) {
                    pos += 12 + static_cast<int>(chunkLen);
                    continue;
                }
                QByteArray textBytes = chunkData.mid(translatedEnd + 1);
                if (compressionFlag == 1) {
                    if (compressionMethod != 0) {
                        pos += 12 + static_cast<int>(chunkLen);
                        continue;
                    }
                    textBytes = qUncompress(textBytes);
                    if (textBytes.isEmpty()) {
                        pos += 12 + static_cast<int>(chunkLen);
                        continue;
                    }
                }
                value = QString::fromUtf8(textBytes);
            } else if (isZTXt) {
                QByteArray chunkData = data.mid(dataStart, chunkLen);
                int nullPos = chunkData.indexOf('\0');
                if (nullPos < 0) { pos += 12 + static_cast<int>(chunkLen); continue; }
                QByteArray kw = chunkData.left(nullPos);
                if (nullPos + 2 > chunkData.size()) { pos += 12 + static_cast<int>(chunkLen); continue; }
                QByteArray compressed = chunkData.mid(nullPos + 2);
                QByteArray decompressed = qUncompress(compressed);
                if (decompressed.isEmpty()) { pos += 12 + static_cast<int>(chunkLen); continue; }
                key = QString::fromUtf8(kw);
                int vn = decompressed.indexOf('\0');
                value = (vn < 0) ? QString::fromUtf8(decompressed) : QString::fromUtf8(decompressed.constData(), vn);
            } else {
                int nullPos = data.indexOf('\0', dataStart);
                if (nullPos < 0 || nullPos >= dataEnd) { pos += 12 + static_cast<int>(chunkLen); continue; }
                key = QString::fromUtf8(raw + dataStart, nullPos - dataStart);
                value = QString::fromUtf8(raw + nullPos + 1, dataEnd - nullPos - 1);
            }

            if (key == QLatin1String("parameters") || key == QLatin1String("prompt")) {
                found = true;
                outMeta.rawJson += value + '\n';

                QStringList lines = value.split('\n');
                if (!lines.isEmpty())
                    outMeta.prompt = lines[0].trimmed();
                for (int i = 1; i < lines.size(); i++) {
                    QString line = lines[i].trimmed();
                    if (line.startsWith("Negative prompt:", Qt::CaseInsensitive)) {
                        outMeta.negativePrompt = line.mid(16).trimmed();
                    } else {
                        QStringList parts = line.split(',');
                        for (const auto &part : parts) {
                            auto kv = part.trimmed().split(':');
                            if (kv.size() != 2) continue;
                            QString k = kv[0].trimmed().toLower();
                            QString v = kv[1].trimmed();

                            if (k == QLatin1String("steps")) outMeta.steps = v.toInt();
                            else if (k == QLatin1String("seed")) outMeta.seed = v.toLongLong();
                            else if (k == QLatin1String("cfg scale")) outMeta.cfgScale = v.toDouble();
                            else if (k == QLatin1String("model")) outMeta.modelName = v;
                            else if (k == QLatin1String("sampler")) outMeta.sampler = v;
                        }
                    }
                }
            }
        }

        pos += 12 + static_cast<int>(chunkLen);
        if (ct[0] == 'I' && ct[1] == 'E' && ct[2] == 'N' && ct[3] == 'D') break;
    }

    if (found)
        outMeta.source = QStringLiteral("stable-diffusion");
    return found;
}

Metadata SDParser::parseFromPNGChunks(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    // Check the signature first: reading a whole JPEG/WEBP only to fail the
    // PNG check is pure I/O waste during a folder scan.
    static const QByteArray kPngSignature("\x89PNG\r\n\x1a\n", 8);
    if (file.read(8) != kPngSignature)
        return {};
    if (!file.seek(0))
        return {};

    QByteArray data = file.readAll();
    Metadata meta;
    if (tryParseFromData(data, meta))
        return meta;
    return {};
}
