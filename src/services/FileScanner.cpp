#include "FileScanner.h"
#include "core/ImageFormats.h"
#include "parsers/MetadataParser.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QUuid>
#include <QDateTime>
#include <QFile>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QPointer>
#include <memory>

FileScanner::FileScanner(QObject *parent)
    : QObject(parent)
{
}

void FileScanner::scanDirectory(const QString &dirPath, bool recursive)
{
    emit scanStarted();
    QPointer<FileScanner> guard(this);
    QtConcurrent::run([guard, dirPath, recursive]() {
        FileScanner *self = guard.data();
        if (!self) return;
        auto assets = std::make_shared<QVector<ScannedAsset>>(self->scanDirectorySync(dirPath, recursive));
        self = guard.data();
        if (!self) return;
        int total = assets->size();
        for (int i = 0; i < total; i++) {
            self = guard.data();
            if (!self) return;
            QMetaObject::invokeMethod(self, [guard, assets, i, total]() {
                FileScanner *self = guard.data();
                if (!self) return;
                emit self->assetFound(assets->at(i));
                emit self->scanProgress(i + 1, total);
            }, Qt::QueuedConnection);
        }
        self = guard.data();
        if (self)
            QMetaObject::invokeMethod(self, [guard]() {
                FileScanner *self = guard.data();
                if (!self) return;
                emit self->scanFinished();
            }, Qt::QueuedConnection);
    });
}

QVector<ScannedAsset> FileScanner::scanDirectorySync(const QString &dirPath, bool recursive) const
{
    QVector<ScannedAsset> assets;
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(dirPath, flags);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (!fi.isFile()) continue;

        QString ext = fi.suffix().toLower();
        if (!isSupportedImageFormat(ext)) continue;

        ScannedAsset item;
        Asset &asset = item.asset;
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

        item.metadata = MetadataParser::parse(asset.filePath);
        asset.metadataSource = item.metadata.source;
        asset.metadataPrompt = item.metadata.prompt;

        assets.append(item);
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
    int dot = filePath.lastIndexOf('.');
    return isSupportedImageFormat(dot >= 0 ? filePath.mid(dot + 1) : QString());
}

QString FileScanner::detectFormat(const QString &filePath)
{
    int dot = filePath.lastIndexOf('.');
    return (dot >= 0 ? filePath.mid(dot + 1) : QString()).toLower();
}
