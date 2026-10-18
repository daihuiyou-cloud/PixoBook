#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QPixmap>
#include <QImage>
#include <QHash>
#include <QString>
#include <QMutex>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "core/IImageCache.h"

class ImageCache : public IImageCache
{
    Q_OBJECT
public:
    explicit ImageCache(qint64 maxBytes = 256LL * 1024 * 1024, QObject *parent = nullptr);

    QPixmap get(const QString &filePath, const QSize &size) const override;
    void insert(const QString &filePath, const QSize &size, const QPixmap &pixmap) override;
    void invalidate(const QString &filePath) override;
    void invalidateDir(const QString &dirPath) override;
    void clear() override;

    void requestThumbnail(const QString &filePath, const QSize &size) override;
    QPixmap generateThumbnail(const QString &filePath, const QSize &size) const override;

public:
    struct CacheKey {
        QString filePath;
        QSize size;
        bool operator==(const CacheKey &other) const {
            return size == other.size && filePath == other.filePath;
        }
    };
    struct CacheKeyHash {
        size_t operator()(const CacheKey &k) const {
            return qHash(k.filePath) ^ qHash(k.size.width()) ^ qHash(k.size.height());
        }
    };
    struct CacheKeyHasher {
        size_t operator()(const CacheKey &k) const {
            return static_cast<size_t>(qHash(k.filePath) ^ qHash(k.size.width()) ^ qHash(k.size.height()));
        }
    };

private:
    QHash<CacheKey, QPixmap> m_cache;
    mutable std::list<CacheKey> m_accessOrder;
    mutable std::unordered_map<CacheKey, std::list<CacheKey>::iterator, CacheKeyHasher> m_orderIndex;
    // Keys with an in-flight background decode, so repeated paint requests do
    // not spawn one decode job per frame.
    std::unordered_set<CacheKey, CacheKeyHasher> m_pending;
    qint64 m_maxBytes;
    mutable qint64 m_currentBytes = 0;
    mutable QMutex m_mutex;

    void touchLru(const CacheKey &key) const;
    void removeFromLru(const CacheKey &key) const;
    void insertLocked(const CacheKey &key, const QPixmap &pixmap);
    void finishThumbnail(const QString &filePath, const QSize &size, const QPixmap &pixmap);
    // Decodes/fits the image without touching QPixmap, so it is safe to call
    // from a worker thread. The caller converts the result on the GUI thread.
    static QImage renderThumbnail(const QString &filePath, const QSize &size);

    static qint64 pixmapBytes(const QPixmap &p) {
        if (p.isNull()) return 0;
        int d = p.depth();
        return static_cast<qint64>(p.width()) * static_cast<qint64>(p.height()) * (d > 0 ? d / 8 : 4);
    }
};

inline uint qHash(const ImageCache::CacheKey &key, uint seed = 0)
{
    return qHash(key.filePath, seed) ^ qHash(key.size.width()) ^ qHash(key.size.height());
}

#endif
