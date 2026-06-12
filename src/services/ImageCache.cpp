#include "ImageCache.h"
#include <QImage>
#include <QPainter>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtConcurrent>

ImageCache::ImageCache(int maxEntries, QObject *parent)
    : QObject(parent), m_maxEntries(maxEntries)
{
}

QPixmap ImageCache::get(const QString &filePath, const QSize &size) const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.value({filePath, size});
}

void ImageCache::insert(const QString &filePath, const QSize &size, const QPixmap &pixmap)
{
    QMutexLocker lock(&m_mutex);
    if (m_cache.size() >= m_maxEntries) {
        int toRemove = qMax(1, m_cache.size() / 4);
        auto keys = m_cache.keys();
        for (int i = 0; i < toRemove && i < keys.size(); i++)
            m_cache.remove(keys[i]);
    }
    m_cache.insert({filePath, size}, pixmap);
}

void ImageCache::invalidate(const QString &filePath)
{
    QMutexLocker lock(&m_mutex);
    QList<CacheKey> toRemove;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        if (it.key().filePath == filePath)
            toRemove.append(it.key());
    }
    for (const auto &key : toRemove)
        m_cache.remove(key);
}

void ImageCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
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
    if (!QFileInfo::exists(filePath))
        return {};

    QImage img(filePath);
    if (img.isNull())
        return {};

    QImage scaled = img.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap result(size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    int x = (size.width() - scaled.width()) / 2;
    int y = (size.height() - scaled.height()) / 2;
    p.drawImage(x, y, scaled);
    p.end();

    return result;
}
