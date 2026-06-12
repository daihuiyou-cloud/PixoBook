#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include "models/Asset.h"

class FileScanner : public QObject
{
    Q_OBJECT
public:
    explicit FileScanner(QObject *parent = nullptr);

    void scanDirectory(const QString &dirPath, bool recursive = true);
    static Asset scanSingleFile(const QString &filePath);
    static bool isSupportedFormat(const QString &filePath);
    static QString computeHash(const QString &filePath);
    static QString detectFormat(const QString &filePath);

signals:
    void assetFound(const Asset &asset);
    void scanProgress(int scanned, int total);
    void scanFinished();

private:
    QVector<Asset> scanDirectorySync(const QString &dirPath, bool recursive);
};

#endif
