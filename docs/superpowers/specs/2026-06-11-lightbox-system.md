# Lightbox System — AI Material Library V3

## 1. Overview

Add a **fullscreen overlay lightbox** to the AI Material Library app, providing an immersive large-image preview experience. When triggered, a `QPainter`-drawn overlay covers the entire client area of the MainWindow, displaying the image large, with zoom, pan, and keyboard/mouse navigation.

## 2. Architecture

- New `LightboxWidget : QWidget` as a direct child of `MainWindow`
- When inactive, `hide()` and receive no events
- When active, `raise() + show() + setFocus()`, intercepting all keyboard and mouse events
- Communicates with `MainWindow` via Qt signals

## 3. Data Flow

```
GalleryWidget::assetDoubleClicked(asset)
    → MainWindow sets lightbox->show(assets, currentIndex)
        → LightboxWidget takes over keyboard/mouse
        → User navigates, zooms, operates
        → Favorite toggle → signal → MainWindow → DB
        → Esc → signal closed() → MainWindow returns focus to gallery
```

## 4. Trigger / Exit

| Action | Enter | Exit |
|--------|-------|------|
| Keyboard | Space / Enter (on selected asset) | Esc |
| Mouse | Double-click gallery card | Top-bar × button |
| Auto-load | Start from gallery.selectedAsset() | — |

## 5. Image Rendering

- **Fit view**: Center image, `min(containerW/imgW, containerH/imgH)` proportional scaling with letterboxing
- **Scroll zoom**: Centered on mouse cursor, ×1.1 / ÷0.9, range 0.1x–10x
- **Pan**: Allowed when zoom > 1.05x, cursor changes to hand
- **Navigation reset**: Zoom=1.0, pan=(0,0) when switching images
- **R key**: Reset zoom to fit

## 6. Navigation

| Operation | Behavior |
|-----------|----------|
| ← / ↑ | Previous image |
| → / ↓ | Next image |
| Home / End | First / Last |
| Click left 50% of image | Previous |
| Click right 50% of image | Next |
| Bottom bar ◀ ▶ buttons | Same as above |

## 7. Overlay Bars

**Top bar** (semi-transparent dark background):
```
[filename]                                   [×]
```

**Bottom bar** (semi-transparent dark background):
```
[◀] [1280×720 | PNG | 2.4MB] [★ Favorite] [100%] [N/M] [▶]
```

- Bars shown on mouse move, auto-fade after 2 seconds of inactivity
- All buttons have precise clickable rects

## 8. Keyboard Shortcuts (Lightbox active)

| Key | Function |
|-----|----------|
| Esc | Exit lightbox |
| ←↑→↓ | Navigate |
| F | Toggle favorite |
| R | Reset zoom |
| Home / End | First / Last |

## 9. Favorite Toggle

Favorite toggle in lightbox → emits `favoriteToggled(assetId, bool)` → `MainWindow::onFavoriteToggled` → DB write → gallery refresh → lightbox sync

## 10. File Changes

| File | Action |
|------|--------|
| `src/ui/LightboxWidget.h` | New |
| `src/ui/LightboxWidget.cpp` | New |
| `src/ui/MainWindow.h` | Add `LightboxWidget *m_lightbox` member |
| `src/ui/MainWindow.cpp` | Create lightbox, connect signals, wire double-click |

## 11. Testing

Manual verification scenarios:
- Space/Enter enter and Esc exit
- ← → navigation with wrap-around at boundaries
- Scroll zoom centered on mouse + pan drag
- Favorite toggle syncs back to gallery
- Lightbox closes, gallery retains focus and selection
