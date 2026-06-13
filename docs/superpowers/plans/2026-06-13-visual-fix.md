# Visual Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply P0 visual fixes: sidebar section hover, DetailPanel spacing/scrollbar, Gallery label height, TitleBar control hover, SearchBar QSS migration, Lightbox timer.

**Architecture:** QPainter-based custom rendering in Qt5/C++. All changes are visual-only, no data model or service changes.

**Tech Stack:** Qt 5.15+, C++17, CMake

**Verification:** Build and run the application. No automated UI tests — visual inspection required.

---

### Task 1: Sidebar section header hover

**Files:**
- Modify: `src/ui/SidebarWidget.h`
- Modify: `src/ui/SidebarWidget.cpp`

- [ ] **Step 1: Add hover tracking state to header**

In `src/ui/SidebarWidget.h`, add to private section after `bool m_hoveredAddTagButton = false;`:
```cpp
bool m_hoveredFolderHeader = false;
bool m_hoveredTagHeader = false;
```

- [ ] **Step 2: Implement hover detection**

In `src/ui/SidebarWidget.cpp`, in `mouseMoveEvent`, before the existing hover detection block, add:
```cpp
QRect folderHeaderRect(0, 0, width(), kSectionHeight);
QRect tagHeaderRect(0, m_sectionTagY, width(), kSectionHeight);
m_hoveredFolderHeader = folderHeaderRect.contains(event->pos());
m_hoveredTagHeader = tagHeaderRect.contains(event->pos());
```

Update the repaint condition to include new hover states:
```cpp
if (oldHoverFolder != m_hoveredFolder || oldHoverTag != m_hoveredTag
    || oldHoverAdd != m_hoveredAddButton || oldHoverAddTag != m_hoveredAddTagButton
    || m_hoveredFolderHeader != oldHoverFolderHeader
    || m_hoveredTagHeader != oldHoverTagHeader)
```

Save old values before the block:
```cpp
bool oldHoverFolderHeader = m_hoveredFolderHeader;
bool oldHoverTagHeader = m_hoveredTagHeader;
```

- [ ] **Step 3: Draw hover background in paintEvent**

In `src/ui/SidebarWidget.cpp`, in `paintEvent`, after `QRect folderHeader(0, 0, width(), kSectionHeight);` add:
```cpp
if (m_hoveredFolderHeader)
    p.fillRect(folderHeader, Color::BG_HOVER);
```

Before `QRect tagHeader(0, m_sectionTagY, width(), kSectionHeight);` add:
```cpp
if (m_hoveredTagHeader)
    p.fillRect(QRect(0, m_sectionTagY, width(), kSectionHeight), Color::BG_HOVER);
```

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . --config Release
```
Run the app and verify folder/tag section headers get a hover background.

---

### Task 2: DetailPanel field spacing

**File:** `src/ui/DetailPanel.cpp`

- [ ] **Step 1: Change row spacing from 21 to 24**

In `drawField` method, change `y += 21` to `y += 24`:
```cpp
void DetailPanel::drawField(QPainter &p, int x, int &y, const QString &label, const QString &value, int labelW)
{
    // ... existing code ...
    y += 24;  // was 21
}
```

- [ ] **Step 2: Build and verify**

Verify margins between metadata/file-info rows have increased.

---

### Task 3: Gallery label height 34→40

**Files:**
- Modify: `src/ui/GalleryWidget.h`
- Modify: `src/ui/GalleryWidget.cpp`

- [ ] **Step 1: Update constant**

In `src/ui/GalleryWidget.h`, change:
```cpp
static constexpr int kLabelHeight = Visual::GalleryLabelHeight;
```
The `Visual::GalleryLabelHeight` is 34. We override locally. Change to:
```cpp
static constexpr int kLabelHeight = 40;
```

- [ ] **Step 2: Verify label rect in paintEvent**

In `src/ui/GalleryWidget.cpp`, the label rect calculation at line ~499 should be correct since it uses `kLabelHeight`. No further changes needed — the label and meta rects derive from `kPadding + m_thumbSize + 8` offset plus `kLabelHeight` for the total card height.

Wait — check `m_itemHeight()`:
```cpp
int m_itemHeight() const { return m_thumbSize + kPadding * 2 + kLabelHeight; }
```
This is correct. Total card height will increase by 6px, labels will have more room.

- [ ] **Step 3: Build and verify**

Check that file names in gallery cards are less frequently truncated.

---

### Task 4: DetailPanel scroll indicator

**Files:**
- Modify: `src/ui/DetailPanel.h`
- Modify: `src/ui/DetailPanel.cpp`

- [ ] **Step 1: Add scroll indicator drawing method to header**

In `src/ui/DetailPanel.h`, add to private section:
```cpp
void drawScrollIndicator(QPainter &p);
```

- [ ] **Step 2: Implement scroll indicator**

In `src/ui/DetailPanel.cpp`, add after `clampScrollOffset()`:
```cpp
void DetailPanel::drawScrollIndicator(QPainter &p)
{
    if (m_maxScrollOffset <= 0) return;
    int indicatorH = qMax(30, height() * height() / (height() + m_maxScrollOffset));
    int indicatorY = (height() - indicatorH) * m_scrollOffset / m_maxScrollOffset;
    int indicatorX = width() - 4;
    QRect indicatorRect(indicatorX, indicatorY, 3, indicatorH);
    p.setBrush(Color::SCROLLBAR);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(indicatorRect, 2, 2);
}
```

- [ ] **Step 3: Call it in paintEvent**

In `paintEvent` of `src/ui/DetailPanel.cpp`, right after `clampScrollOffset()` at the end of the paint method, add:
```cpp
drawScrollIndicator(p);
```

- [ ] **Step 4: Build and verify**

Open the DetailPanel with an asset that has long metadata. The scroll indicator should appear on the right edge when content overflows.

---

### Task 5: TitleBar control button hover consistency

**File:** `src/ui/TitleBar.cpp`

- [ ] **Step 1: Add rounded rect hover for min/max/close buttons**

In `paintEvent`, replace:
```cpp
if (m_hoveredControl == CtrlMinimize) p.fillRect(minRect, Color::BG_SELECTED);
```
with:
```cpp
if (m_hoveredControl == CtrlMinimize) {
    QPainterPath minPath;
    minPath.addRoundedRect(QRectF(minRect.adjusted(4, 2, -4, -2)), Visual::RadiusSmall, Visual::RadiusSmall);
    p.fillPath(minPath, Color::BG_SELECTED);
}
```

Same for maximize button:
```cpp
if (m_hoveredControl == CtrlMaximize) {
    QPainterPath maxPath;
    maxPath.addRoundedRect(QRectF(maxRect.adjusted(4, 2, -4, -2)), Visual::RadiusSmall, Visual::RadiusSmall);
    p.fillPath(maxPath, Color::BG_SELECTED);
}
```

For close button:
```cpp
if (m_hoveredControl == CtrlClose) {
    QPainterPath closePath;
    closePath.addRoundedRect(QRectF(closeRect.adjusted(4, 2, -4, -2)), Visual::RadiusSmall, Visual::RadiusSmall);
    p.fillPath(closePath, Color::CLOSE_HOVER);
    Codicon::draw(p, "chrome-close", closeRect, Qt::white, 14);
} else {
    Codicon::draw(p, "chrome-close", closeRect, Color::TEXT_PRIMARY, 14);
}
```

- [ ] **Step 2: Build and verify**

Hover over minimize/maximize/close buttons in the title bar — they should show rounded hover backgrounds.

---

### Task 6: Migrate QSS summaryPill to QPainter

**Files:**
- Modify: `src/ui/SearchBar.cpp`
- Modify: `resources/resources.qrc`
- Delete: `resources/styles/main.qss`
- Modify: `src/main.cpp`

- [ ] **Step 1: Remove QSS `summaryPill` from SearchBar**

In `src/ui/SearchBar.cpp`, remove the line:
```cpp
m_resultSummary->setProperty("summaryPill", true);
```

Add custom paint for the summary label by subclassing QLabel or using a custom widget. The simpler approach: remove the `summaryPill` property and use QPainter in SearchBar's `paintEvent`.

In `SearchBar::paintEvent`, after drawing the bottom line, add:
```cpp
// Draw summary pill background
if (!m_resultSummary->text().isEmpty()) {
    QRect sr = m_resultSummary->geometry();
    QPainterPath pillPath;
    pillPath.addRoundedRect(QRectF(sr), Visual::RadiusSmall, Visual::RadiusSmall);
    p.fillPath(pillPath, Color::BG_BUTTON_SUBTLE);
    p.setPen(QPen(Color::BORDER_MUTED, 1));
    p.drawPath(pillPath);
}
```

Also need to remove `setAutoFillBackground(true)` from the summary label to avoid painting conflicts.

- [ ] **Step 2: Remove main.qss file reference**

Delete `resources/styles/main.qss`.

In `resources/resources.qrc`, remove the line:
```xml
<file>styles/main.qss</file>
```

- [ ] **Step 3: Remove QSS loading from main.cpp**

In `src/main.cpp`, remove or comment out the QSS loading block:
```cpp
// Remove this entire block:
// QFile styleFile(":/styles/main.qss");
// ...
// app.setStyleSheet(qss);
```

Keep the QToolTip stylesheet line:
```cpp
app.setStyleSheet(QStringLiteral("QToolTip { font-family: \"Microsoft YaHei UI\"; font-size: 10pt; color: #cccccc; background-color: #2d2d2d; border: 1px solid #404040; padding: 4px 8px; }"));
```

- [ ] **Step 4: Build and verify**

The result summary pill should look the same (or better) than before.

---

### Task 7: Lightbox overlay timer

**File:** `src/ui/LightboxWidget.cpp`

- [ ] **Step 1: Change timer interval**

In the constructor of LightboxWidget, change:
```cpp
m_overlayTimer->setInterval(2000);
```
to:
```cpp
m_overlayTimer->setInterval(3000);
```

- [ ] **Step 2: Build and verify**

Open lightbox — the overlay should now stay visible for 3 seconds instead of 2.

---

### Task 8: Build and final verification

- [ ] **Step 1: Full build**

```bash
cd build && cmake --build . --config Release
```

- [ ] **Step 2: Visual checklist**

1. Sidebar section headers show hover background (Task 1)
2. DetailPanel field rows have more spacing (Task 2)
3. Gallery card labels have more vertical room (Task 3)
4. DetailPanel shows scroll indicator on right edge when content overflows (Task 4)
5. TitleBar control buttons have rounded hover backgrounds (Task 5)
6. Summary pill in SearchBar renders correctly (Task 6)
7. Lightbox overlay stays visible 3s (Task 7)
8. No QSS-related warnings in console output (Task 6)
