# Visual Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refine the visual quality of the AI Material Library dark-theme UI — spacing, shadows, card rendering, sidebar polish, DetailPanel layout, and micro-interactions.

**Architecture:** Modify `VisualConstants.h` for spacing/elevation constants; update QPainter-based rendering in GalleryWidget, SidebarWidget, DetailPanel, ActivityBar, TitleBar. No structural or architectural changes.

**Tech Stack:** Qt5 (Widgets), C++17, QPainter custom rendering

---

### Task 1: Update VisualConstants.h — spacing and elevation

**Files:**
- Modify: `src/ui/VisualConstants.h`

- [ ] **Step 1: Modify row/section heights and add elevation constants**

In `src/ui/VisualConstants.h`, change:
- `RowHeight` from `28` to `26`
- `SectionHeight` from `30` to `28`

Add after `DetailPanelWidth`:
```cpp
inline constexpr int ElevationNone = 0;
inline constexpr int ElevationLow  = 1;
inline constexpr int ElevationMid  = 2;
```


### Task 2: Update GalleryWidget constants

**Files:**
- Modify: `src/ui/GalleryWidget.h`

- [ ] **Step 1: Update kPadding, kGap, kLabelHeight**

In `src/ui/GalleryWidget.h`, change:
- `kPadding = 12` → `kPadding = 14`
- `kGap = 12` → `kGap = 14`
- `kLabelHeight = 46` → `kLabelHeight = 38`


### Task 3: GalleryWidget card rendering — shadow, selection, star, borders

**Files:**
- Modify: `src/ui/GalleryWidget.cpp`

- [ ] **Step 1: Add shadow drawing for hovered/selected cards**

In the paintEvent loop, after the card background fill and before the border, add shadow for hovered/selected cards. Find the block (around line 214-221):
```cpp
        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(r), Visual::RadiusMedium, Visual::RadiusMedium);
        p.fillPath(cardPath, isSelected ? Color::BG_CARD_HOVER
                                        : (isHovered ? Color::BG_CARD_HOVER : Color::BG_CARD));
        p.setPen(QPen(isSelected ? Color::ACCENT
                                 : (isHovered ? Color::BORDER_SUBTLE : Color::BORDER_MUTED),
                      isSelected ? 2 : 1));
        p.drawPath(cardPath);
```

Replace with:
```cpp
        if (isSelected || isHovered) {
            QRect shadowRect = r.adjusted(2, 2, 2, 6);
            p.setPen(Qt::NoPen);
            p.setBrush(Color::OVERLAY_SHADOW);
            p.drawRoundedRect(shadowRect, Visual::RadiusMedium, Visual::RadiusMedium);
        }

        QPainterPath cardPath;
        cardPath.addRoundedRect(QRectF(r), Visual::RadiusMedium, Visual::RadiusMedium);
        p.fillPath(cardPath, isSelected ? Color::BG_CARD_HOVER
                                        : (isHovered ? Color::BG_CARD_HOVER : Color::BG_CARD));
        p.setPen(QPen(isSelected ? Color::ACCENT
                                 : (isHovered ? Color::BORDER_SUBTLE : Color::BORDER_FAINT),
                      isSelected ? 2 : 1));
        p.drawPath(cardPath);

        if (isSelected) {
            p.setPen(QPen(Color::WHITE_15, 1));
            p.drawLine(r.left() + 4, r.top() + 2, r.right() - 4, r.top() + 2);
        }
```

- [ ] **Step 2: Update star icon rect size**

Find the star rect (around line 270):
```cpp
        QRect starRect(r.right() - 34, r.bottom() - 34, 26, 26);
```
Replace with:
```cpp
        QRect starRect(r.right() - 40, r.bottom() - 40, 32, 32);
```

Also update the click hit-test area in `mousePressEvent` (around line 350):
```cpp
        QRect starRect(r.right() - 42, r.bottom() - 42, 38, 38);
```

- [ ] **Step 3: Update label and meta font sizes**

Find (around line 253-267):
```cpp
        QRect labelRect(r.left() + kPadding, r.top() + kPadding + m_thumbSize + 8,
                        r.width() - kPadding * 2 - 28, 18);
```
Change `- 28` to `- 32` to account for larger star area, and `18` to `16`:
```cpp
        QRect labelRect(r.left() + kPadding, r.top() + kPadding + m_thumbSize + 8,
                        r.width() - kPadding * 2 - 32, 16);
```

Find the metaRect:
```cpp
        QRect metaRect(labelRect.left(), labelRect.bottom() + 2, r.width() - kPadding * 2 - 28, 16);
```
Change to:
```cpp
        QRect metaRect(labelRect.left(), labelRect.bottom() + 1, r.width() - kPadding * 2 - 32, 14);
```


### Task 4: SidebarWidget polish — chevron size, divider color, empty states

**Files:**
- Modify: `src/ui/SidebarWidget.cpp`

- [ ] **Step 1: Increase chevron icon size**

In `paintEvent`, find the folder header chevron draw (around line 86):
```cpp
    Codicon::draw(p, m_foldersExpanded ? "chevron-down" : "chevron-right",
                  QRect(8, 0, 14, kSectionHeight), Color::TEXT_SECONDARY, 12);
```
Change `12` to `14`.

Find the tag header chevron (around line 132):
```cpp
    Codicon::draw(p, m_tagsExpanded ? "chevron-down" : "chevron-right",
                  QRect(8, tagHeader.top(), 14, kSectionHeight), Color::TEXT_SECONDARY, 12);
```
Change `12` to `14`.

- [ ] **Step 2: Soften divider lines**

Find both `p.drawLine(12, ..., width() - 12, ...)` lines and change pen from `Color::BORDER` to `Color::BORDER_MUTED`.

- [ ] **Step 3: Increase folder/tag icon sizes**

In `drawFolderIcon`, change the draw call size from `15` to `16`:
```cpp
    Codicon::draw(p, "folder", QRect(r.x() + 12, r.y(), 18, r.height()), fc, 16);
```

In `drawTagDot`, change the draw call size from `14` to `16`:
```cpp
    Codicon::draw(p, "tag", QRect(center.x() - 8, center.y() - 8, 16, 16), color, 16);
```

- [ ] **Step 4: Improve add-folder button hover style**

In `paintEvent`, find the add button hover fill (around line 92):
```cpp
    if (m_hoveredAddButton)
        p.fillRect(addRect, Color::BG_SELECTED);
```
Change `BG_SELECTED` to `BG_BUTTON_HOVER`.


### Task 5: DetailPanel polish — close button, image info bar, fields, tag pills

**Files:**
- Modify: `src/ui/DetailPanel.cpp`

- [ ] **Step 1: Enlarge close button and add hover effect**

Find (around line 71):
```cpp
    m_closeBtnRect = QRect(width() - 30, 8, 22, 22);
    Codicon::draw(p, "close", m_closeBtnRect, Color::TEXT_SECONDARY, 14);
```
Replace with:
```cpp
    m_closeBtnRect = QRect(width() - 34, 6, 26, 26);
    bool closeHovered = m_closeBtnRect.contains(mapFromGlobal(QCursor::pos()));
    if (closeHovered) {
        QColor closeBg = Color::CLOSE_HOVER;
        closeBg.setAlpha(30);
        p.fillRect(m_closeBtnRect, closeBg);
    }
    Codicon::draw(p, "close", m_closeBtnRect, closeHovered ? Color::CLOSE_HOVER : Color::TEXT_SECONDARY, 15);
```

- [ ] **Step 2: Add gradient overlay to image info bar**

Find (around line 122-129):
```cpp
    QRect infoBg(imgRect.left(), imgRect.bottom() - 30, imgRect.width(), 30);
    p.fillRect(infoBg, Color::OVERLAY_LIGHT);
```
Replace with:
```cpp
    QRect infoBg(imgRect.left(), imgRect.bottom() - 30, imgRect.width(), 30);
    QLinearGradient grad(infoBg.topLeft(), infoBg.topRight());
    grad.setColorAt(0.0, Color::OVERLAY_LIGHT);
    grad.setColorAt(1.0, Qt::transparent);
    p.fillRect(infoBg, grad);
```

- [ ] **Step 3: Standardize field label width**

In all `drawField` calls, ensure label width is `64`. Find calls like:
```cpp
    drawField(p, 16, y, QStringLiteral("名称"), m_asset.fileName);
    drawField(p, 16, y, QStringLiteral("大小"), ...);
    drawField(p, 16, y, QStringLiteral("尺寸"), ...);
    drawField(p, 16, y, QStringLiteral("格式"), ...);
    drawField(p, 16, y, QStringLiteral("修改时间"), ...);
```
Change to use explicit 64px label width:
```cpp
    drawField(p, 16, y, QStringLiteral("名称"), m_asset.fileName, 64);
    drawField(p, 16, y, QStringLiteral("大小"), QString::number(m_asset.fileSize / 1024) + " KB", 64);
    drawField(p, 16, y, QStringLiteral("尺寸"), QString("%1 x %2").arg(m_asset.width).arg(m_asset.height), 64);
    drawField(p, 16, y, QStringLiteral("格式"), m_asset.format.toUpper(), 64);
    drawField(p, 16, y, QStringLiteral("修改时间"), fi.lastModified().toString("yyyy-MM-dd hh:mm"), 64);
```

- [ ] **Step 4: Increase tag pill radius**

Find all `drawRoundedRect(tagBg, Visual::RadiusSmall, Visual::RadiusSmall)` calls and change to `Visual::RadiusMedium`.

- [ ] **Step 5: Enlarge tag close button**

Find:
```cpp
    Codicon::draw(p, "close", QRect(tagBg.right() - 16, tagBg.top() + 3, 12, 16),
                  Color::TEXT_PRIMARY, 9);
```
Replace with:
```cpp
    Codicon::draw(p, "close", QRect(tagBg.right() - 18, tagBg.top() + 2, 14, 18),
                  Color::TEXT_PRIMARY, 10);
```


### Task 6: TitleBar and ActivityBar polish

**Files:**
- Modify: `src/ui/TitleBar.cpp`
- Modify: `src/ui/ActivityBar.cpp`

- [ ] **Step 1: Increase TitleBar menu icon size**

In `TitleBar.cpp`, find (around line 80):
```cpp
    Codicon::draw(p, m_menus[i].iconName, QRect(r.left() + 8, r.top(), 16, r.height()),
                  textColor, 12);
```
Change `12` to `14`.

- [ ] **Step 2: Add rounded hover background for ActivityBar**

In `ActivityBar.cpp`, find (around line 58-59):
```cpp
        else if (hovered)
            p.fillRect(r.adjusted(4, 6, -4, -6), Color::BG_HOVER);
```
Replace with:
```cpp
        else if (hovered) {
            QPainterPath hoverPath;
            hoverPath.addRoundedRect(QRectF(r.adjusted(4, 6, -4, -6)), Visual::RadiusSmall, Visual::RadiusSmall);
            p.fillPath(hoverPath, Color::BG_HOVER);
        }
```

Add include at top of `ActivityBar.cpp`:
```cpp
#include <QPainterPath>
```


### Task 7: Build and verify

- [ ] **Step 1: Build project**

```bash
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

Expected: clean build, no errors or warnings.

- [ ] **Step 2: Verify visual changes**

Launch the executable and verify:
- Gallery cards show shadow on hover/selection
- Star icon area is larger and more comfortable to click
- Card labels are more compact (38px vs 46px)
- Sidebar dividers are softer, chevrons slightly larger
- DetailPanel close button has hover feedback
- Image info bar uses gradient overlay
- Metadata fields are left-aligned consistently
- Tag pills use larger radius
- ActivityBar hover backgrounds are rounded
- TitleBar menu icons are slightly larger
