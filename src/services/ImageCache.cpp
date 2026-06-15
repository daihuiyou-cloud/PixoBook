#include "ImageCache.h"
#include <QDir>
#include <QImageReader>
#include <QPainter>
#include <QMutexLocker>
#include <QtConcurrent>

ImageCache::ImageCache(qint64 maxBytes, QObject *parent)
    : IImageCache(parent), m_maxBytes(maxBytes)
{
}

QPixmap ImageCache::get(const QString &filePath, const QSize &size) const
{
    QMutexLocker lock(&m_mutex);
    CacheKey key{filePath, size};
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it.value();
    return {};
}

void ImageCache::insert(const QString &filePath, const QSize &size, const QPixmap &pixmap)
{
    QMutexLocker lock(&m_mutex);
    CacheKey key{filePath, size};
    qint64 newBytes = pixmapBytes(pixmap);
    if (m_cache.contains(key)) {
        m_currentBytes -= pixmapBytes(m_cache[key]);
        m_cache[key] = pixmap;
        m_currentBytes += newBytes;
        m_accessOrder.removeAll(key);
        m_accessOrder.append(key);
        return;
    }
    while (m_currentBytes + newBytes > m_maxBytes && !m_accessOrder.isEmpty()) {
        CacheKey oldest = m_accessOrder.takeFirst();
        if (m_cache.contains(oldest)) {
            m_currentBytes -= pixmapBytes(m_cache[oldest]);
            m_cache.remove(oldest);
        }
    }
    m_cache.insert(key, pixmap);
    m_currentBytes += newBytes;
    m_accessOrder.append(key);
}

void ImageCache::invalidate(const QString &filePath)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it.key().filePath == filePath) {
            m_currentBytes -= pixmapBytes(it.value());
            m_accessOrder.removeAll(it.key());
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
        if (it.key().filePath.startsWith(prefix)) {
            m_currentBytes -= pixmapBytes(it.value());
            m_accessOrder.removeAll(it.key());
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
}

void ImageCache::requestThumbnail(const QString &filePath, const QSize &size)
{
    QPixmap cached = get(filePath, size);
    if (!cached.isNull()) {
        emit thumbnailReady(filePath, size, cached);
        return;
    }

    QtConcurrent::run([this, filePath, size]() {
        QPixmap thumb = generateThumbnail(filePath, size);
        insert(filePath, size, thumb);
        emit thumbnailReady(filePath, size, thumb);
    });
}

QPixmap ImageCache::generateThumbnail(const QString &filePath, const QSize &size) const
{
    QImageReader reader(filePath);
    QSize imgSize = reader.size();
    if (!imgSize.isValid())
        return {};

    QSize fitSize = imgSize.scaled(size, Qt::KeepAspectRatio);
    if (fitSize != imgSize)
        reader.setScaledSize(fitSize);

    QImage img = reader.read();
    if (img.isNull())
        return {};

    if (img.size() != fitSize)
        img = img.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap result(size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    int x = (size.width() - img.width()) / 2;
    int y = (size.height() - img.height()) / 2;
    p.drawImage(x, y, img);
    p.end();

    return result;
}
