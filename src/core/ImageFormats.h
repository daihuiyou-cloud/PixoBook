#ifndef IMAGEFORMATS_H
#define IMAGEFORMATS_H

#include <QStringList>

inline const QStringList& supportedImageFormats()
{
    static const QStringList formats = {"png", "jpg", "jpeg", "webp"};
    return formats;
}

inline bool isSupportedImageFormat(const QString &suffix)
{
    return supportedImageFormats().contains(suffix.toLower());
}

#endif
