# Visual Polish — AI Material Library

## 1. Overview

Refine the existing dark-theme UI of the AI Material Library desktop application (Qt5/C++, QPainter-based custom rendering) to improve overall visual quality. The color system, spacing constants, typography scale, and component architecture remain intact; changes are focused on rendering quality, consistency, and micro-interactions.

## 2. Scope

| Area | Changes |
|------|---------|
| **Typography & spacing** | Standardize use of `Visual::Space*` constants; reduce card label height 46→38px; adjust paddings/gaps 12→14px in gallery |
| **Card/Gallery** | Add bottom shadow hint to cards; refine selection indicator; increase star hit area 26→32px; use faintest border by default |
| **Shadow/elevation** | Introduce `ElevationNone/Low/Mid` constants; draw soft shadow under hovered/selected cards |
| **Sidebar** | Bump chevron icon size 12→14px; unify folder/tag icon sizes; make divider lines softer |
| **DetailPanel** | Enlarge close button hit area; add gradient overlay to image info bar; align metadata field label width; increase tag pill radius 4→6px |
| **Empty/special states** | Add Codicon prefix to "no items" placeholder text; use smaller secondary font |
| **Micro-interactions** | No property animations (Qt5 limitation); use immediate visual state changes with clearer hover/active differentiation |

## 3. Non-Goals

- No font size changes (current scale is accepted)
- No architecture or layout restructuring
- No new widgets or features
- No performance optimization

## 4. Constants Changes (VisualConstants.h)

```cpp
// Modified
inline constexpr int RowHeight = 26;          // 28→26
inline constexpr int SectionHeight = 28;      // 30→28

// New
inline constexpr int ElevationNone = 0;
inline constexpr int ElevationLow  = 1;
inline constexpr int ElevationMid  = 2;
```

## 5. Component Changes

### 5.1 GalleryWidget

- `kLabelHeight`: 46 → 38
- `kPadding`: 12 → 14
- `kGap`: 12 → 14
- Star icon rect: 26×26 → 32×32 (reposition to `r.right() - 36, r.bottom() - 36`)
- Shadow: draw `OVERLAY_SHADOW` rect offset (2,2,2,6) under hovered/selected card
- Default border: `BORDER_FAINT` instead of `BORDER_MUTED` for unselected
- Hovered border: `BORDER_SUBTLE`
- Selected border: `ACCENT` at 1.5px width
- Selection indicator: retain 3px left bar, add top 1px `WHITE_15` highlight line

### 5.2 SidebarWidget

- Chevron icon size: `12` → `14`
- Folder icon drawing: `15` → `16` in `drawFolderIcon`
- Tag icon drawing: `14` → `16` in `drawTagDot`
- Section divider pen: `Color::BORDER` → `Color::BORDER_MUTED`
- Empty-state text: draw at `FontMeta` with preceding Codicon icon
- Add-folder button: hover background `BG_SELECTED` → `BG_BUTTON_HOVER`

### 5.3 DetailPanel

- Close button area: `22×22` → `26×26` rect, hover fill red at reduced opacity
- Image info bar: replace flat `OVERLAY_LIGHT` fill with linear gradient (left opaque → right transparent)
- Section headers: add subtle hover background highlight
- Field label width: standardize to 64px in `drawField` calls
- Tag pill radius: use `RadiusMedium` (6px) instead of `RadiusSmall` (4px)
- Tag close button: enlarge from 12×16 to 14×18

### 5.4 TitleBar

- Menu icon size: `12` → `14`
- Menu item hover background: add `RadiusSmall` rounded rect

### 5.5 ActivityBar

- Icon hover background: add `RadiusSmall` rounded rect

## 6. File Changes

| File | Change type |
|------|-------------|
| `src/ui/VisualConstants.h` | Modify spacing constants, add elevation constants |
| `src/ui/GalleryWidget.cpp` | Card rendering, shadow, star area, label height |
| `src/ui/GalleryWidget.h` | Update `kLabelHeight` / `kPadding` / `kGap` constants |
| `src/ui/SidebarWidget.cpp` | Icon/chevron sizes, divider color, empty states |
| `src/ui/DetailPanel.cpp` | Close btn, gradient overlay, field alignment, tag pills |
| `src/ui/ActivityBar.cpp` | Rounded hover background |
| `src/ui/TitleBar.cpp` | Menu icon size, hover radius |
| `src/ui/ColorConstants.h` | (No changes needed; existing colors sufficient) |

## 7. Testing

Visual verification only — no new unit tests. Run the application and verify:
- Gallery cards show subtle shadow on hover/selection
- Star icon hit area feels comfortable
- DetailPanel sections have consistent field alignment
- Sidebar dividers appear softer
- No clipping or visual artifacts
