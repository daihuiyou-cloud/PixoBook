# UI Module Phase 2 Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete remaining UI module performance and UX optimizations — lazy pagination, ImageCache byte-based eviction, drag feedback, Toast parent-following, DetailPanel partial update, Gallery multi-select fix, and Settings removal.

**Architecture:** All changes are localized to specific files. Database layer gets pagination params (offset/limit). Gallery gets incremental loading triggered by scroll proximity. ImageCache switches from entry-count to byte-based LRU. No new classes needed.

**Tech Stack:** Qt5 Widgets, C++17, SQLite (via QSqlQuery)

---

### Task 1: ImageCache — byte-based LRU eviction

**Files:**
- Modify: `src/services/ImageCache.h`
- Modify: `src/services/ImageCache.cpp`

Current: evicts by entry count (500 max). QPixmap(280,280) and QPixmap(80,80) each count as 1 entry, 12x memory waste. New: track total bytes, evict LRU past m_maxBytes (default 256MB).

- [ ] **Step 1: Modify ImageCache.h**

Replace `int m_maxEntries` with `qint64 m_maxBytes` + `mutable qint64 m_currentBytes = 0`. Add static helper `pixmapBytes()`. Update constructor default to `qint64 maxBytes = 256 * 1024 * 1024`.

- [ ] **Step 2: Rewrite insert() eviction logic**

In `insert()`: before inserting new entry, while `m_currentBytes + newBytes > m_maxBytes`, evict LRU and subtract its bytes. On hit, subtract old bytes before replacing. In `clear()`: reset `m_currentBytes = 0`. In `invalidate()`: subtract bytes on erase.

- [ ] **Step 3: Update MainWindow.cpp constructor call**

Change `new ImageCache(500, this)` → `new ImageCache(256 * 1024 * 1024, this)`.

- [ ] **Step 4: Build and verify**

```
cd build && cmake --build . --config Release
```

- [ ] **Step 5: Commit**

```
git add -A && git commit -m "perf(cache): ImageCache 按字节大小LRU淘汰(默认256MB)"
```

---

### Task 2: Database pagination support

**Files:**
- Modify: `src/core/IDatabaseManager.h`
- Modify: `src/database/DatabaseManager.h`
- Modify: `src/database/DatabaseManager.cpp`
- Modify: `src/services/LibraryController.h`
- Modify: `src/services/LibraryController.cpp`

Add `offset` and `limit` params to `searchAssets()`. Defaults: `offset=0, limit=-1` (no limit).

- [ ] **Step 1: Update IDatabaseManager.h signature**

```cpp
virtual QVector<Asset> searchAssets(const QString &keyword, const QString &source,
                                    const QVector<int> &tagIds, bool onlyFavorites,
                                    const QString &sortField = "created_at",
                                    bool sortAscending = false,
                                    int offset = 0, int limit = -1) const = 0;
```

- [ ] **Step 2: Update DatabaseManager.h — match signature**

- [ ] **Step 3: Update DatabaseManager.cpp — append `LIMIT ? OFFSET ?` when limit > 0**

```cpp
if (limit > 0)
    sql += QStringLiteral(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
```

- [ ] **Step 4: Update LibraryController.h/.cpp — pass through offset/limit**

- [ ] **Step 5: Build**

- [ ] **Step 6: Commit**

```
git add -A && git commit -m "feat(db): searchAssets 支持 LIMIT/OFFSET 分页"
```

---

### Task 3: GalleryWidget — incremental loading (lazy pagination)

**Files:**
- Modify: `src/ui/GalleryWidget.h`
- Modify: `src/ui/GalleryWidget.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`

Load first 200 assets, then detect scroll proximity to bottom to load more. Prevents loading 10000+ assets at once.

- [ ] **Step 1: Add pagination state to GalleryWidget.h**

Add `kPageSize = 200`, `m_totalAssetCount = 0`, method `appendPage()`, method `checkLoadMore()`, signal `loadMoreRequested(int offset, int limit)`.

- [ ] **Step 2: Modify setAssets signature**

`void setAssets(const QVector<Asset> &assets, int totalCount)` — store totalCount in `m_totalAssetCount`. Keep `appendAssets` for backward compat.

- [ ] **Step 3: Update wheelEvent to trigger load**

In `wheelEvent()`, after offset clamping, call `checkLoadMore()` which emits `loadMoreRequested` when within 3 rows of bottom and `m_assets.size() < m_totalAssetCount`.

- [ ] **Step 4: Wire up in MainWindow**

Add `m_currentOffset` member. Modify `loadAssets()` to track offset and pass it through. Connect `loadMoreRequested` signal to load the next page and call `m_gallery->appendPage()`.

- [ ] **Step 5: Add countAssets to IDatabaseManager + DatabaseManager**

`int countAssets(...)` = `SELECT COUNT(*)` with same WHERE clause. Wire through LibraryController.

- [ ] **Step 6: Build**

- [ ] **Step 7: Commit**

```
git add -A && git commit -m "feat(gallery): 懒加载分页(每页200张,滚动底部自动加载)"
```

---

### Task 4: DetailPanel — 局部重绘优化

**Files:**
- Modify: `src/ui/DetailPanel.cpp`

Same pattern as GalleryWidget Phase 1. Currently `mouseMoveEvent` calls `update()` on every hover state change. Only update the affected button rects.

- [ ] **Step 1: Replace `update()` calls with `update(rect)`**

```cpp
if (oldAddHover != m_addTagHovered) update(m_addTagRect);
if (oldCopyHover != m_copyPromptHovered) update(m_copyPromptRect);
if (oldCloseHover != m_closeBtnHovered) update(m_closeBtnRect);
```

Also use `update(imageArea())` for pan tracking instead of `update()`.

- [ ] **Step 2: Build**

- [ ] **Step 3: Commit**

```
git add -A && git commit -m "perf(detail): mouseMoveEvent 改为局部 update()"
```

---

### Task 5: GalleryWidget — 多选 + 键盘导航冲突修复

**Files:**
- Modify: `src/ui/GalleryWidget.cpp`

Current: `navigateTo()` clears `m_selectedIndices`. After Ctrl+Click multi-select, arrow keys lose all selections.

- [ ] **Step 1: Fix navigateTo to preserve multi-select**

```cpp
void GalleryWidget::navigateTo(int index)
{
    if (index < 0 || index >= m_assets.size()) return;
    m_selectedAsset = m_assets[index];
    if (!m_selectedIndices.contains(index)) {
        m_selectedIndices.clear();
        m_selectedIndices.insert(index);
    }
    QRect r = itemRect(index);
    if (r.top() < 0) m_scrollOffset += r.top();
    else if (r.bottom() > height()) m_scrollOffset += r.bottom() - height();
    m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, m_totalHeight - height()));
    update();
    emit assetSelected(m_selectedAsset);
}
```

- [ ] **Step 2: Build**

- [ ] **Step 3: Commit**

```
git add -A && git commit -m "fix(gallery): 键盘导航保留多选状态"
```

---

### Task 6: 拖拽视觉反馈

**Files:**
- Modify: `src/ui/GalleryWidget.h`
- Modify: `src/ui/GalleryWidget.cpp`

Add hover border highlight when dragging files over the gallery.

- [ ] **Step 1: Add m_isDragOver member to GalleryWidget.h**

```cpp
bool m_isDragOver = false;
```

- [ ] **Step 2: Override dragEnterEvent/dragMoveEvent/dragLeaveEvent/dropEvent**

`dragEnterEvent/dragMoveEvent`: set `m_isDragOver = true`, accept action.
`dragLeaveEvent`: set `m_isDragOver = false`, update.
`dropEvent`: set `m_isDragOver = false` at start.

- [ ] **Step 3: Draw overlay in paintEvent**

After background fill, if `m_isDragOver`:
```cpp
QPen dashPen(Color::ACCENT, 2, Qt::DashLine);
p.setPen(dashPen);
p.setBrush(QColor(Color::ACCENT.red(), Color::ACCENT.green(), Color::ACCENT.blue(), 20));
p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);
```

- [ ] **Step 4: Build**

- [ ] **Step 5: Commit**

```
git add -A && git commit -m "feat(gallery): 拖拽文件进入时显示虚线高亮反馈"
```

---

### Task 7: Toast 窗口跟随父窗口

**Files:**
- Modify: `src/ui/ToastNotification.h`
- Modify: `src/ui/ToastNotification.cpp`

Toast position is fixed at creation. Fix: install event filter on parent to reposition on Move events.

- [ ] **Step 1: Add reposition() + eventFilter to ToastNotification**

```cpp
// In header:
    void reposition();
    bool eventFilter(QObject *obj, QEvent *event) override;
```

```cpp
void ToastNotification::reposition()
{
    if (!parentWidget()) return;
    QPoint pos = parentWidget()->mapToGlobal(
        QPoint(parentWidget()->width() - width() - 16, 50));
    move(pos);
}

bool ToastNotification::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == parentWidget() && event->type() == QEvent::Move)
        reposition();
    return QWidget::eventFilter(obj, event);
}
```

- [ ] **Step 2: Install filter in showMessage, remove on cleanup**

In `showMessage()`: add `parentWidget()->installEventFilter(this)` after setting position.
In `deleteLater` path (timer callback): add `if (parentWidget()) parentWidget()->removeEventFilter(this);`.

- [ ] **Step 3: Build**

- [ ] **Step 4: Commit**

```
git add -A && git commit -m "fix(toast): 父窗口移动时自动跟随定位"
```

---

### Task 8: 移除 Settings 按钮

**Files:**
- Modify: `src/ui/ActivityBar.h`
- Modify: `src/ui/ActivityBar.cpp`
- Modify: `src/ui/MainWindow.cpp`

Remove the unused Settings icon and its dead code path.

- [ ] **Step 1: Reduce ActivityBar::Activity enum**

```cpp
// old: enum Activity { Gallery, Favorites, Tags, Settings };
// new: enum Activity { Gallery, Favorites, Tags };
```

- [ ] **Step 2: Remove Settings icon from m_icons (4th entry)**

- [ ] **Step 3: Remove `case ActivityBar::Settings:` block from MainWindow**

- [ ] **Step 4: Build**

- [ ] **Step 5: Commit**

```
git add -A && git commit -m "refactor(ui): 移除未实现的Settings按钮"
```

---

## Verification

- [ ] **Final build check:** `cmake --build build --config Release`
- [ ] **All existing tests pass:** `cd build && ctest -C Release`
