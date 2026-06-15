#include "FileScanner.h"
#include "core/ImageFormats.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QUuid>
#include <QDateTime>
#include <QFile>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QPointer>

FileScanner::FileScanner(QObject *parent)
    : QObject(parent)
{
}

void FileScanner::scanDirectory(const QString &dirPath, bool recursive)
{
    QPointer<FileScanner> guard(this);
    QtConcurrent::run([guard, dirPath, recursive]() {
        FileScanner *self = guard.data();
        if (!self) return;
        auto assets = self->scanDirectorySync(dirPath, recursive);
        int total = assets.size();
        for (int i = 0; i < assets.size(); i++) {
            emit self->assetFound(assets[i]);
            emit self->scanProgress(i + 1, total);
        }
        emit self->scanFinished();
    });
}

QVector<Asset> FileScanner::scanDirectorySync(const QString &dirPath, bool recursive) const
{
    QVector<Asset> assets;
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(dirPath, flags);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (!fi.isFile()) continue;

        QString ext = fi.suffix().toLower();
        if (!isSupportedImageFormat(ext)) continue;

        Asset asset;
        asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        asset.filePath = fi.absoluteFilePath();
        asset.fileName = fi.fileName();
        asset.fileSize = fi.size();
        asset.format = ext;
        asset.folderId = fi.absolutePath();
        asset.createdAt = fi.birthTime();
        asset.updatedAt = fi.lastModified();

        QImageReader reader(asset.filePath);
        QSize dims = reader.size();
        if (dims.isValid()) {
            asset.width = dims.width();
            asset.height = dims.height();
        }

        asset.hash = computeHash(asset.filePath);

        assets.append(asset);
    }

    return assets;
}

QString FileScanner::computeHash(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&f))
        return hash.result().toHex();
    return {};
}

Asset FileScanner::scanSingleFile(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.isFile()) return {};

    QString ext = fi.suffix().toLower();
    if (!isSupportedImageFormat(ext)) return {};

    Asset asset;
    asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.filePath = fi.absoluteFilePath();
    asset.fileName = fi.fileName();
    asset.fileSize = fi.size();
    asset.format = ext;
    asset.folderId = fi.absolutePath();
    asset.createdAt = fi.birthTime();
    asset.updatedAt = fi.lastModified();

    QImageReader reader(asset.filePath);
    QSize dims = reader.size();
    if (dims.isValid()) {
        asset.width = dims.width();
        asset.height = dims.height();
    }

    asset.hash = computeHash(asset.filePath);
    return asset;
}

bool FileScanner::isSupportedFormat(const QString &filePath)
{
    return isSupportedImageFormat(QFileInfo(filePath).suffix());
}

QString FileScanner::detectFormat(const QString &filePath)
{
    return QFileInfo(filePath).suffix().toLower();
}
