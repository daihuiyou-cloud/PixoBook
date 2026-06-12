#ifndef IIMAGECACHE_H
#define IIMAGECACHE_H

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QObject>

class IImageCache : public QObject
{
    Q_OBJECT
public:
    explicit IImageCache(QObject *parent = nullptr) : QObject(parent) {}
    ~IImageCache() override = default;

    virtual QPixmap get(const QString &filePath, const QSize &size) const = 0;
    virtual void insert(const QString &filePath, const QSize &size, const QPixmap &pixmap) = 0;
    virtual void invalidate(const QString &filePath) = 0;
    virtual void clear() = 0;
    virtual void requestThumbnail(const QString &filePath, const QSize &size) = 0;
    virtual QPixmap generateThumbnail(const QString &filePath, const QSize &size) const = 0;

signals:
    void thumbnailReady(const QString &filePath, const QSize &size, const QPixmap &pixmap);
};

#endif
