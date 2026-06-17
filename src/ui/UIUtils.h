#ifndef UIUTILS_H
#define UIUTILS_H

#include <QPalette>
#include <QMenu>
#include <QString>
#include "ui/ColorConstants.h"

namespace UIUtils {

inline QString displayNameForSource(const QString &source)
{
    if (source == QLatin1String("stable-diffusion")) return QStringLiteral("Stable Diffusion");
    if (source == QLatin1String("midjourney")) return QStringLiteral("Midjourney");
    if (source == QLatin1String("dalle")) return QStringLiteral("DALL-E");
    return source;
}

inline void setupMenuPalette(QMenu &menu)
{
    QPalette pal;
    pal.setColor(QPalette::Window, Color::BG_DARK);
    pal.setColor(QPalette::WindowText, Color::TEXT_PRIMARY);
    pal.setColor(QPalette::Highlight, Color::HIGHLIGHT);
    pal.setColor(QPalette::HighlightedText, Color::TEXT_PRIMARY);
    menu.setPalette(pal);
}

} // namespace UIUtils

#endif
