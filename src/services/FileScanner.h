#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QObject>
#include <QSet>
#include <QVector>
#include <QMetaType>
#include "models/Asset.h"
#include "models/Metadata.h"

struct ScannedAsset {
    Asset asset;
    Metadata metadata;
};

Q_DECLARE_METATYPE(ScannedAsset)

class FileScanner : public QObject
{
    Q_OBJECT
public:
    explicit FileScanner(QObject *parent = nullptr);

    // knownPaths lets a rescan skip hash/metadata work for already-imported
    // files; they are filtered out before any expensive I/O.
    void scanDirectory(const QString &dirPath, bool recursive = true,
                       const QSet<QString> &knownPaths = {});
    static Asset scanSingleFile(const QString &filePath);
    [[nodiscard]] static bool isSupportedFormat(const QString &filePath);
    static QString computeHash(const QString &filePath);
    static QString detectFormat(const QString &filePath);

signals:
    void scanStarted();
    void assetFound(const ScannedAsset &item);
    void scanProgress(int scanned, int total);
    void scanFinished();

private:
    QVector<ScannedAsset> scanDirectorySync(const QString &dirPath, bool recursive,
                                            const QSet<QString> &knownPaths) const;
};

#endif
