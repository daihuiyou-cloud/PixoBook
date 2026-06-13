# Visual Fixes — AI Material Library

## 1. Overview

Quick visual fixes for the AI Material Library desktop application based on code review. Focused on P0-level issues: consistency gaps, minor bugs, and polish regressions.

## 2. P0 Changes

### 2.1 Sidebar section header hover feedback

**File:** `src/ui/SidebarWidget.cpp`

- Add hover background on section header (素材文件夹 / 标签)
- Track `m_hoveredSection` and apply `BG_HOVER` on hover
- Makes headers feel clickable (matching VSCode Explorer behavior)

### 2.2 DetailPanel field spacing

**File:** `src/ui/DetailPanel.cpp`

- `drawField`: row spacing 21px → 24px to match `Visual::SpaceMd`
- Provides more breathing room in metadata/file info sections

### 2.3 Gallery label height

**Files:** `src/ui/GalleryWidget.h`, `src/ui/GalleryWidget.cpp`

- `kLabelHeight` 34px → 40px
- Prevents file name truncation on most assets

### 2.4 Scroll indicator in DetailPanel

**File:** `src/ui/DetailPanel.cpp`

- Draw subtle scrollbar indicator on the right edge when content exceeds visible area
- Thin rounded rect, same style as existing CustomStyle scrollbar

### 2.5 TitleBar control button hover consistency

**File:** `src/ui/TitleBar.cpp`

- Window control buttons (minimize/maximize/close): use rounded hover background
- Match `RadiusSmall` and `BG_SELECTED` for min/max; keep `CLOSE_HOVER` for close

### 2.6 SearchBar style drift

**Files:** `src/ui/SearchBar.cpp`, `resources/styles/main.qss`

- Migrate `summaryPill` from QSS to QPainter rendering
- Remove entirely the `main.qss` file (sole rule moved into code)

### 2.7 Lightbox overlay timer

**File:** `src/ui/LightboxWidget.cpp`

- Overlay auto-hide timer: 2000ms → 3000ms
- More forgiving for reading image metadata

## 3. Non-Goals

- No new features or widgets
- No architecture changes
- No font size or color palette changes
- No perf optimization

## 4. Files Changed

| File | Change |
|------|--------|
| `src/ui/SidebarWidget.h` | Add `bool m_hoveredSection = false; int m_hoveredSectionIndex = -1;` |
| `src/ui/SidebarWidget.cpp` | Track section header hover; draw hover background |
| `src/ui/DetailPanel.cpp` | Row spacing 21→24; add scroll indicator |
| `src/ui/DetailPanel.h` | Add `drawScrollIndicator()` |
| `src/ui/GalleryWidget.h` | `kLabelHeight` 34→40 |
| `src/ui/GalleryWidget.cpp` | Update `kLabelHeight` usage |
| `src/ui/TitleBar.cpp` | Rounded hover for control buttons |
| `src/ui/SearchBar.cpp` | QPainter-rendered summary pill |
| `resources/styles/main.qss` | Remove (content migrated) |
| `resources/resources.qrc` | Remove main.qss reference if present |
| `src/ui/LightboxWidget.cpp` | Timer 2000→3000 |
| `src/main.cpp` | Remove QSS loading if present |
