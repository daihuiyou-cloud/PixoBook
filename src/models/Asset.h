#ifndef ASSET_H
#define ASSET_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

struct Asset {
    QString id;
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    int width = 0;
    int height = 0;
    QString format;
    QString hash;
    bool isFavorite = false;
    QString folderId;
    QString metadataSource;
    QString metadataPrompt;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool operator==(const Asset &other) const { return id == other.id; }
    bool operator!=(const Asset &other) const { return id != other.id; }
};

Q_DECLARE_METATYPE(Asset)

#endif
