# UI Improvement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix font clarity, increase window size, and add icons across the Qt5 UI

**Architecture:** Modify existing QPainter-based custom widgets to use larger fonts, enable Qt5 high-DPI scaling, and leverage the already-loaded Codicon font for icons currently rendered as hand-drawn lines or Unicode

**Tech Stack:** Qt5 (Widgets), C++17, Codicon icon font

---

### Task 1: High-DPI Scaling and Window Size

**Files:**
- Modify: `src/main.cpp:7-22`
- Modify: `src/ui/MainWindow.cpp:591-594`

- [ ] **Step 1: Enable Qt5 high-DPI scaling in main.cpp**

Edit `src/main.cpp`. Before `QApplication app(argc, argv)`, add:

```cpp
#if defined(Q_OS_WIN)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
```

Also change the default window size from 1280×800 to 1600×1000:

```cpp
// was: window.resize(1280, 800);
    window.resize(1600, 1000);
```

- [ ] **Step 2: Increase minimum window size in MainWindow.cpp**

In `MainWindow.cpp`, find `mmi->ptMinTrackSize` and change:

```cpp
// was: mmi->ptMinTrackSize.x = 800; mmi->ptMinTrackSize.y = 600;
        mmi->ptMinTrackSize.x = 1000;
        mmi->ptMinTrackSize.y = 700;
```

- [ ] **Step 3: Verify build**

Run: `cd build && cmake .. -G "MinGW Makefiles" (or your generator) && cmake --build .`
Expected: builds without errors

---

### Task 2: Increase Codicon Default Font Size

**Files:**
- Modify: `src/ui/Codicon.h:12`

- [ ] **Step 1: Bump default font size**

In `Codicon.h`, change `font(int pixelSize = 16)` to `font(int pixelSize = 22)`:

```cpp
    static QFont font(int pixelSize = 22) {
```

- [ ] **Step 2: Bump icon size in ActivityBar**

In `src/ui/ActivityBar.cpp:63`, change `Codicon::draw(p, m_icons[i].iconName, r.adjusted(4, 4, -4, -4), iconColor, 20)` to use `24`:

```cpp
        Codicon::draw(p, m_icons[i].iconName, r.adjusted(4, 4, -4, -4), iconColor, 24);
```

- [ ] **Step 3: Verify build**

Run build. Expected: builds clean.

---

### Task 3: Increase Font Sizes in CustomStyle

**Files:**
- Modify: `src/ui/CustomStyle.cpp:180,201,226,304`

- [ ] **Step 1: CE_PushButton text font**

In `CustomStyle.cpp:180`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 2: CE_MenuBarItem text font**

In `CustomStyle.cpp:201`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 3: CE_MenuItem text font**

In `CustomStyle.cpp:226`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 4: CC_ComboBox text font**

In `CustomStyle.cpp:304`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 5: Verify build**

---

### Task 4: Increase Font Sizes in MainWindow Status Bar

**Files:**
- Modify: `src/ui/MainWindow.cpp:199`

- [ ] **Step 1: Status bar font**

In `MainWindow.cpp:199`, change `sf.setPixelSize(12)` to `sf.setPixelSize(14)`.

- [ ] **Step 2: Verify build**

---

### Task 5: Increase Font Sizes in TitleBar

**Files:**
- Modify: `src/ui/TitleBar.cpp:31,77`

- [ ] **Step 1: Menu item metrics font (title metrics)**

In `TitleBar.cpp:31`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 2: Menu text paint font**

In `TitleBar.cpp:77`, change `f.setPixelSize(12)` to `f.setPixelSize(14)`.

- [ ] **Step 3: App logo icon size**

In `TitleBar.cpp:74`, change `Codicon::draw(p, "layout", QRect(8, 6, 20, 20), QColor(0xcc, 0xcc, 0xcc), 14)` - change the rect to `QRect(8, 5, 24, 22)` and size to `18`:

```cpp
    Codicon::draw(p, "layout", QRect(8, 5, 24, 22), QColor(0xcc, 0xcc, 0xcc), 18);
```

- [ ] **Step 4: Verify build**

---

### Task 6: Increase Font Sizes and Icons in SidebarWidget

**Files:**
- Modify: `src/ui/SidebarWidget.cpp:76,78,88,102,133,135,146`

- [ ] **Step 1: Section header font**

Line 76: `sf.setPixelSize(12)` → `sf.setPixelSize(14)`

Line 102: `f.setPixelSize(12)` → `f.setPixelSize(14)` (folder items font)

Line 133: `sf2.setPixelSize(12)` → `sf2.setPixelSize(14)` (tag section header)

Line 146: `f2.setPixelSize(12)` → `f2.setPixelSize(14)` (tag items font)

- [ ] **Step 2: Replace section header Unicode triangles with Codicon chevrons**

Line 78: Replace `QString(m_foldersExpanded ? QChar(0x25BC) : QChar(0x25B6))` with Codicon chevron:

```cpp
        QString folderHeader = QString(m_foldersExpanded ? Codicon::icon("chevron-down") : Codicon::icon("chevron-right"))
                             + "  " + QStringLiteral("文件夹");
```

Line 135: Same for tags:

```cpp
    QString tagHeader = QString(m_tagsExpanded ? Codicon::icon("chevron-down") : Codicon::icon("chevron-right"))
                      + "  " + QStringLiteral("标签");
```

- [ ] **Step 3: Increase folder icon size**

Line 54: `Codicon::draw(p, "folder", QRect(r.x() + 4, r.y(), 20, r.height()), fc, 14)` → change size from `14` to `16`:

```cpp
    Codicon::draw(p, "folder", QRect(r.x() + 4, r.y(), 20, r.height()), fc, 16);
```

Line 59: `Codicon::draw(p, "tag", QRect(center.x() - 7, center.y() - 7, 14, 14), color, 12)` → change to `16, 16` rect and `14` size:

```cpp
    Codicon::draw(p, "tag", QRect(center.x() - 8, center.y() - 8, 16, 16), color, 14);
```

- [ ] **Step 4: Verify build**

---

### Task 7: Increase Font Sizes and Icons in SearchBar

**Files:**
- Modify: `src/ui/SearchBar.cpp:23,26-32,43,69,72-86,98`

- [ ] **Step 1: Input and combo font sizes**

Line 23: `inputFont.setPixelSize(12)` → `inputFont.setPixelSize(14)`
Line 43: `comboFont.setPixelSize(12)` → `comboFont.setPixelSize(14)`
Line 69: already uses `inputFont` — no change needed

- [ ] **Step 2: Search icon size**

Lines 26-32: Increase search icon pixmap and draw size from 14 to 16:

```cpp
    QPixmap searchPx(22, 22);
    searchPx.fill(Qt::transparent);
    {
        QPainter sp(&searchPx);
        Codicon::draw(sp, "search", QRect(0, 0, 22, 22), QColor(0x96, 0x96, 0x96), 16);
    }
```

- [ ] **Step 3: Thumbnail size buttons → Codicon icons**

Replace lines 72-86. Change from Unicode block characters to Codicon icons:

```cpp
    m_sizeSmallBtn = new QPushButton();
    m_sizeSmallBtn->setCheckable(true);
    m_sizeSmallBtn->setToolTip(QString::fromUtf8("\xe5\xb0\x8f\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (100px)"));
    QPixmap smallPx(16, 16);
    smallPx.fill(Qt::transparent);
    { QPainter sp(&smallPx); Codicon::draw(sp, "zoom-in", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
    m_sizeSmallBtn->setIcon(QIcon(smallPx));
    layout->addWidget(m_sizeSmallBtn);

    m_sizeMediumBtn = new QPushButton();
    m_sizeMediumBtn->setCheckable(true);
    m_sizeMediumBtn->setChecked(true);
    m_sizeMediumBtn->setToolTip(QString::fromUtf8("\xe4\xb8\xad\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (180px)"));
    QPixmap medPx(16, 16);
    medPx.fill(Qt::transparent);
    { QPainter sp(&medPx); Codicon::draw(sp, "screen-normal", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
    m_sizeMediumBtn->setIcon(QIcon(medPx));
    layout->addWidget(m_sizeMediumBtn);

    m_sizeLargeBtn = new QPushButton();
    m_sizeLargeBtn->setCheckable(true);
    m_sizeLargeBtn->setToolTip(QString::fromUtf8("\xe5\xa4\xa7\xe7\xbc\xa9\xe7\x95\xa5\xe5\x9b\xbe (280px)"));
    QPixmap largePx(16, 16);
    largePx.fill(Qt::transparent);
    { QPainter sp(&largePx); Codicon::draw(sp, "screen-full", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
    m_sizeLargeBtn->setIcon(QIcon(largePx));
    layout->addWidget(m_sizeLargeBtn);
```

- [ ] **Step 4: Favorites button → Codicon star**

Replace line 98. Change from Unicode to use Codicon icons:

```cpp
    QPixmap favOffPx(16, 16);
    favOffPx.fill(Qt::transparent);
    { QPainter sp(&favOffPx); Codicon::draw(sp, "star-empty", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
    m_favButton = new QPushButton(QIcon(favOffPx), QString::fromUtf8(" \xe6\x94\xb6\xe8\x97\x8f"));
    m_favButton->setCheckable(true);
```

- [ ] **Step 5: Update fav button toggle to use star/star-empty**

Replace lines 113-117:

```cpp
    connect(m_favButton, &QPushButton::toggled, this, [this](bool checked) {
        m_onlyFavorites = checked;
        QPixmap px(16, 16);
        px.fill(Qt::transparent);
        { QPainter sp(&px); Codicon::draw(sp, checked ? "star" : "star-empty", QRect(0, 0, 16, 16), QColor(0xcc, 0xcc, 0xcc), 14); }
        m_favButton->setIcon(QIcon(px));
        m_favButton->setText(checked ? QString::fromUtf8(" \xe6\x94\xb6\xe8\x97\x8f") : QString::fromUtf8(" \xe6\x94\xb6\xe8\x97\x8f"));
        if (m_ready) emit filterChanged();
    });
```

- [ ] **Step 6: Verify build**

---

### Task 8: Replace TitleBar Window Controls with Codicon Icons

**Files:**
- Modify: `src/ui/TitleBar.cpp:91-132`

- [ ] **Step 1: Replace minimize button hand-drawn line with Codicon**

Replace lines 91-99. Change from QPainter line drawing to Codicon:

```cpp
    {
        QRect r = minimizeBtnRect();
        if (m_hoveredControl == 0) {
            p.fillRect(r, QColor(0x37, 0x37, 0x3d));
        }
        Codicon::draw(p, "chrome-minimize", r, QColor(0xcc, 0xcc, 0xcc), 14);
    }
```

- [ ] **Step 2: Replace maximize/restore button with Codicon**

Replace lines 101-119:

```cpp
    {
        QRect r = maximizeBtnRect();
        if (m_hoveredControl == 1) {
            p.fillRect(r, QColor(0x37, 0x37, 0x3d));
        }
        Codicon::draw(p, m_maximized ? "chrome-restore" : "chrome-maximize", r, QColor(0xcc, 0xcc, 0xcc), 14);
    }
```

- [ ] **Step 3: Replace close button with Codicon**

Replace lines 121-132:

```cpp
    {
        QRect r = closeBtnRect();
        if (m_hoveredControl == 2) {
            p.fillRect(r, QColor(0xe8, 0x11, 0x23));
            Codicon::draw(p, "chrome-close", r, Qt::white, 14);
        } else {
            Codicon::draw(p, "chrome-close", r, QColor(0xcc, 0xcc, 0xcc), 14);
        }
    }
```

- [ ] **Step 4: Verify build**

---

### Task 9: Add Icons to TitleBar Menu Items

**Files:**
- Modify: `src/ui/TitleBar.h:54-56`
- Modify: `src/ui/TitleBar.cpp:13-18,66-89`

- [ ] **Step 1: Add iconName to MenuDef struct**

In `TitleBar.h`, modify the `MenuDef` struct:

```cpp
    struct MenuDef {
        QString text;
        QString iconName;
    };
```

- [ ] **Step 2: Add icon names to menu definitions**

In `TitleBar.cpp`, update the menu definitions:

```cpp
    m_menus = {
        { QStringLiteral("\u6587\u4ef6"), "file" },
        { QStringLiteral("\u7f16\u8f91"), "edit" },
        { QStringLiteral("\u67e5\u770b"), "eye" },
        { QStringLiteral("\u5e2e\u52a9"), "question" },
    };
```

- [ ] **Step 3: Draw icons next to menu text**

In `TitleBar.cpp`, in the `paintEvent` method where menus are drawn (around lines 82-89), add icon drawing before the text:

```cpp
    for (int i = 0; i < m_menus.size(); i++) {
        QRect r = menuItemRect(i);
        if (i == m_hoveredMenu) {
            p.fillRect(r, QColor(0x37, 0x37, 0x3d));
        }
        // Draw menu icon
        Codicon::draw(p, m_menus[i].iconName, QRect(r.left() + 4, r.top(), 16, r.height()), 
                      i == m_hoveredMenu ? QColor(0xff, 0xff, 0xff) : QColor(0xcc, 0xcc, 0xcc), 12);
        p.setPen(i == m_hoveredMenu ? QColor(0xff, 0xff, 0xff) : QColor(0xcc, 0xcc, 0xcc));
        p.drawText(r.adjusted(22, 0, 0, 0), Qt::AlignVCenter, m_menus[i].text);
    }
```

- [ ] **Step 4: Verify build**

---

### Task 10: Final Build and Smoke Test

- [ ] **Step 1: Full clean build**

```bash
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

Expected: no compile errors or warnings

- [ ] **Step 2: Launch and verify**

Run the built executable. Verify:
- Window opens at approximately 1600×1000
- Text throughout UI is noticeably larger (14px vs old 12px)
- ActivityBar icons are larger
- TitleBar window buttons show Codicon glyphs (not hand-drawn lines)
- TitleBar menu items have icons
- Sidebar section headers use chevron icons
- SearchBar thumb buttons have Codicon icons, fav button uses star icon
- No visual artifacts or clipping
