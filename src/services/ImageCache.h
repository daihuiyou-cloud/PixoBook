#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QPixmap>
#include <QHash>
#include <QString>
#include <QObject>
#include <QMutex>
#include <functional>

class ImageCache : public QObject
{
    Q_OBJECT
public:
    explicit ImageCache(int maxEntries = 500, QObject *parent = nullptr);

    QPixmap get(const QString &filePath, const QSize &size) const;
    void insert(const QString &filePath, const QSize &size, const QPixmap &pixmap);
    void invalidate(const QString &filePath);
    void clear();

    void requestThumbnail(const QString &filePath, const QSize &size);
    QPixmap generateThumbnail(const QString &filePath, const QSize &size) const;

signals:
    void thumbnailReady(const QString &filePath, const QSize &size, const QPixmap &pixmap);

public:
    struct CacheKey {
        QString filePath;
        QSize size;
        bool operator==(const CacheKey &other) const {
            return filePath == other.filePath && size == other.size;
        }
    };
    struct CacheKeyHash {
        size_t operator()(const CacheKey &k) const {
            return qHash(k.filePath) ^ qHash(k.size.width()) ^ qHash(k.size.height());
        }
    };

private:
    QHash<CacheKey, QPixmap> m_cache;
    int m_maxEntries;
    mutable QMutex m_mutex;
};

inline uint qHash(const ImageCache::CacheKey &key, uint seed = 0)
{
    return qHash(key.filePath, seed) ^ qHash(key.size.width()) ^ qHash(key.size.height());
}

#endif
