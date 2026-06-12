#ifndef COLORCONSTANTS_H
#define COLORCONSTANTS_H

#include <QColor>

namespace Color {

// Backgrounds
inline const QColor BG_DARKEST      = QColor(0x1e, 0x1e, 0x1e);
inline const QColor BG_DARK         = QColor(0x25, 0x25, 0x26);
inline const QColor BG_MEDIUM       = QColor(0x2d, 0x2d, 0x30);
inline const QColor BG_HOVER        = QColor(0x2a, 0x2d, 0x2e);
inline const QColor BG_SELECTED     = QColor(0x37, 0x37, 0x3d);
inline const QColor BG_ACTIVITYBAR  = QColor(0x33, 0x33, 0x33);
inline const QColor BG_INPUT        = QColor(0x3c, 0x3c, 0x3c);
inline const QColor BG_BUTTON_HOVER = QColor(0x4a, 0x4a, 0x4a);
inline const QColor BG_MENUBAR      = QColor(0x32, 0x32, 0x33);
inline const QColor BG_LOADING      = QColor(0x3c, 0x3c, 0x3f);

// Text
inline const QColor TEXT_PRIMARY    = QColor(0xcc, 0xcc, 0xcc);
inline const QColor TEXT_SECONDARY  = QColor(0x96, 0x96, 0x96);
inline const QColor TEXT_BRIGHT     = QColor(0xff, 0xff, 0xff);
inline const QColor TEXT_DISABLED   = QColor(0x50, 0x50, 0x50);

// Accent
inline const QColor ACCENT          = QColor(0x00, 0x7a, 0xcc);
inline const QColor HIGHLIGHT       = QColor(0x09, 0x47, 0x71);

// Borders
inline const QColor BORDER          = QColor(0x3c, 0x3c, 0x3c);
inline const QColor BORDER_INPUT    = QColor(0x5a, 0x5a, 0x5a);
inline const QColor BORDER_CARD     = QColor(0x3c, 0x3c, 0x3c);

// Special
inline const QColor FAVORITE_ON     = QColor(0xff, 0xcc, 0x00);
inline const QColor FAVORITE_OFF    = QColor(0x60, 0x60, 0x60);
inline const QColor CLOSE_HOVER     = QColor(0xe8, 0x11, 0x23);
inline const QColor ERROR_TEXT      = QColor(0xc0, 0x60, 0x60);
inline const QColor ERROR_BG        = QColor(0x3c, 0x20, 0x20);

// Scrollbar
inline const QColor SCROLLBAR       = QColor(0x42, 0x42, 0x42);
inline const QColor SCROLLBAR_HOVER = QColor(0x4f, 0x4f, 0x4f);

// Overlay
inline const QColor OVERLAY_BG      = QColor(0, 0, 0, 180);
inline const QColor OVERLAY_LIGHT   = QColor(0, 0, 0, 160);
inline const QColor OVERLAY_SHADOW  = QColor(0, 0, 0, 60);

// Misc
inline const QColor SEARCH_HIGHLIGHT = QColor(0x09, 0x47, 0x71, 40);
inline const QColor WHITE_15        = QColor(0xff, 0xff, 0xff, 15);

}

#endif
