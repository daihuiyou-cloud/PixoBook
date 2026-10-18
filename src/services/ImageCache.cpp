#include "ImageCache.h"
#include <QDir>
#include <QImageReader>
#include <QPainter>
#include <QMutexLocker>
#include <QtConcurrent>
#include <QPointer>

ImageCache::ImageCache(qint64 maxBytes, QObject *parent)
    : IImageCache(parent), m_maxBytes(maxBytes)
{
}

void ImageCache::touchLru(const CacheKey &key) const
{
    auto oit = m_orderIndex.find(key);
    if (oit != m_orderIndex.end())
        m_accessOrder.splice(m_accessOrder.end(), m_accessOrder, oit->second);
}

void ImageCache::removeFromLru(const CacheKey &key) const
{
    auto oit = m_orderIndex.find(key);
    if (oit != m_orderIndex.end()) {
        m_accessOrder.erase(oit->second);
        m_orderIndex.erase(oit);
    }
}

QPixmap ImageCache::get(const QString &filePath, const QSize &size) const
{
    QMutexLocker lock(&m_mutex);
    CacheKey key{filePath, size};
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        touchLru(key);
        return it.value();
    }
    return {};
}

void ImageCache::insert(const QString &filePath, const QSize &size, const QPixmap &pixmap)
{
    QMutexLocker lock(&m_mutex);
    insertLocked(CacheKey{filePath, size}, pixmap);
}

void ImageCache::insertLocked(const CacheKey &key, const QPixmap &pixmap)
{
    // Never cache a null pixmap: get() treats it as a miss, so it would both
    // skew m_currentBytes and cause endless re-decodes of unreadable files.
    if (pixmap.isNull())
        return;

    qint64 newBytes = pixmapBytes(pixmap);
    if (m_cache.contains(key)) {
        m_currentBytes -= pixmapBytes(m_cache[key]);
        m_cache[key] = pixmap;
        m_currentBytes += newBytes;
        touchLru(key);
        return;
    }
    while (m_currentBytes + newBytes > m_maxBytes && !m_accessOrder.empty()) {
        CacheKey oldest = m_accessOrder.front();
        m_accessOrder.pop_front();
        m_orderIndex.erase(oldest);
        if (m_cache.contains(oldest)) {
            m_currentBytes -= pixmapBytes(m_cache[oldest]);
            m_cache.remove(oldest);
        }
    }
    m_cache.insert(key, pixmap);
    m_currentBytes += newBytes;
    m_accessOrder.push_back(key);
    m_orderIndex[key] = std::prev(m_accessOrder.end());
}

void ImageCache::invalidate(const QString &filePath)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it.key().filePath == filePath) {
            m_currentBytes -= pixmapBytes(it.value());
            removeFromLru(it.key());
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void ImageCache::invalidateDir(const QString &dirPath)
{
    QMutexLocker lock(&m_mutex);
    QString prefix = QDir::fromNativeSeparators(dirPath);
    if (!prefix.endsWith('/'))
        prefix += '/';
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it.key().filePath.startsWith(prefix, Qt::CaseInsensitive)) {
            m_currentBytes -= pixmapBytes(it.value());
            removeFromLru(it.key());
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void ImageCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
    m_accessOrder.clear();
    m_orderIndex.clear();
    m_currentBytes = 0;
}

void ImageCache::requestThumbnail(const QString &filePath, const QSize &size)
{
    QPixmap cached = get(filePath, size);
    if (!cached.isNull()) {
        emit thumbnailReady(filePath, size, cached);
        return;
    }

    const CacheKey key{filePath, size};
    {
        QMutexLocker lock(&m_mutex);
        if (m_pending.find(key) != m_pending.end())
            return; // a decode for this key is already running
        m_pending.insert(key);
    }

    QPointer<ImageCache> guard(this);
    QtConcurrent::run([guard, filePath, size]() {
        const QImage img = renderThumbnail(filePath, size);
        ImageCache *self = guard.data();
        if (!self) return;
        QMetaObject::invokeMethod(self, [guard, filePath, size, img]() {
            ImageCache *self = guard.data();
            if (!self) return;
            // QPixmap is created on the GUI thread; only the QImage decode ran
            // in the worker.
            self->finishThumbnail(filePath, size, QPixmap::fromImage(img));
        }, Qt::QueuedConnection);
    });
}

void ImageCache::finishThumbnail(const QString &filePath, const QSize &size,
                                 const QPixmap &pixmap)
{
    const CacheKey key{filePath, size};
    {
        QMutexLocker lock(&m_mutex);
        m_pending.erase(key);
        insertLocked(key, pixmap);
    }
    emit thumbnailReady(filePath, size, pixmap);
}

QPixmap ImageCache::generateThumbnail(const QString &filePath, const QSize &size) const
{
    return QPixmap::fromImage(renderThumbnail(filePath, size));
}

QImage ImageCache::renderThumbnail(const QString &filePath, const QSize &size)
{
    QImageReader reader(filePath);
    const QSize imgSize = reader.size();
    if (!imgSize.isValid() || size.isEmpty())
        return {};

    const QSize fitSize = imgSize.scaled(size, Qt::KeepAspectRatio);
    if (fitSize != imgSize)
        reader.setScaledSize(fitSize);

    QImage img = reader.read();
    if (img.isNull())
        return {};

    if (img.size() != fitSize)
        img = img.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter p(&result);
    const int x = (size.width() - img.width()) / 2;
    const int y = (size.height() - img.height()) / 2;
    p.drawImage(x, y, img);
    p.end();

    return result;
}
