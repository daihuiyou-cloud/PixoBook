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
    // Set when the file behind this row disappeared from disk. The row (and
    // its tags/favorite) is kept: annotations are primary user data, the file
    // path is only its current address.
    QDateTime missingSince;

    bool isMissing() const { return missingSince.isValid(); }

    bool operator==(const Asset &other) const { return id == other.id; }
    bool operator!=(const Asset &other) const { return id != other.id; }
};

Q_DECLARE_METATYPE(Asset)

#endif
