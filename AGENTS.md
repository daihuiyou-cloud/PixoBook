# PixoBook — 递归自我改进

本文件指导 opencode agent 在代码变更后自动进行静态分析和质量检查。

---

## Anchored Summary

### Goal
- 对 PixoBook 项目进行多轮 C++ 性能优化评审与修复，涵盖数据库查询、I/O、paintEvent 堆分配、虚函数去虚化、数据局部性

### Constraints & Preferences
- 使用 C++17 + Qt5，需兼容 MSVC 编译器
- 扫描器 ci/scan-bug-patterns.ps1 是零依赖 PowerShell 脚本，全部 37 个检查块在其中可用
- 源码修复必须在 src/ 目录下，遵循现有代码风格（m_ 前缀成员、Allman 大括号）
- 已提交 8 个 perf/fix commit (ddd08ee, 7d322af, bda1aeb, 71009ed, a7f92bf, 2164ce5, dcc5b14, dd7b53e)；第 9 轮已完成（工作区变更，尚未提交）

### Progress

#### Done
- **ddd08ee** — paintEvent 热路径零分配 + 10 处性能优化（GalleryWidget/DetailPanel/SidebarWidget 预计算）
- **7d322af** — CacheKey::operator== 短路（size 先于 filePath）、renameTag SQL 化（`getTag()` 替代 `getAllTags()` 全表扫描）、qPrintable 替换、扫描器新增 CHECK 31-33（paintevent-trimmed / qstring-byvalue / qfileinfo-string-only）
- **bda1aeb** — `searchAssets` 窗口函数合并 count+search（`COUNT(*) OVER()` 子查询）、`MainWindow` tags 缓存 + `LibraryController::tagsChanged` 信号、`SDParser::tryParseFromData` 消除二次 PNG 文件读取、20 个类加 `final` 关键字、`Asset.h` 字段重排（QString 连续）
- 全部构建 0 错 0 警告，33/33 扫描器 PASS（2 个预存 QString by-value 接口问题），3/4 测试通过（tst_filestorage 预存问题）
- **本轮（第 4 轮）**：
  - TabBar/TitleBar QFontMetrics 成员缓存，消除每帧临时 QFontMetrics（~6-8 次分配/帧）
  - DetailPanel + LightboxWidget 异步图片加载（`m_loadGeneration` 计数器 + `QtConcurrent::run` 后台解码 + `QPointer` guard）
  - DetailPanel `clampScrollOffset()` 从 paintEvent 移到 resizeEvent（已在 wheelEvent 内联）
  - GalleryWidget 惰性 `QFileInfo::exists()`：删除 `prefetchFileExistence`，paintEvent 中对可见项按需 stat + 缓存
- **71009ed** — 第 4 轮汇总：TabBar/TitleBar QFontMetrics 缓存、DetailPanel/LightboxWidget 异步图片加载、DetailPanel clampScrollOffset 移出 paintEvent、GalleryWidget 惰性 fileExists
- **a7f92bf** — 第 5 轮：DetailPanel/SidebarWidget/GalleryWidget QFontMetrics 成员缓存（~25+ 临时对象/帧 → 0）、DetailPanel `m_zoomText` 预计算、drawEmptyState 静态 QVector、FileScanner/SidebarWidget QFileInfo → 纯字符串操作（CHECK 33 消除）
- 全部构建 0 错 0 警告，32/33 扫描器 PASS（1 个预存 QString by-value 接口问题），3/4 测试通过（tst_filestorage 预存问题）
- **2164ce5** — 第 6 轮：FileWatcher 按目录已知集合隔离（`m_knownByDir`，消除跨目录误删/数据丢失）、undo/redo 后 gallery/detail 刷新（`reloadAfterUndo`）、TabBar/TitleBar/DetailPanel/SidebarWidget QFontMetrics 头文件初始化失效修复（COW 快照）、LibraryController 事务提交失败回滚、GalleryWidget indexAt O(1)、TabBar setCurrentIndex 越界钳制、扫描器 CHECK 32 正则修正（仅参数位置）+ 新增 CHECK 34（QFontMetrics stale member）
- 全部构建 0 错 0 警告，34/34 扫描器 PASS，4/4 测试通过
- **dd7b53e** — 第 8 轮：扫描导入事务批处理（DatabaseManager SAVEPOINT 嵌套事务 + LibraryController scanStarted/scanFinished 括起整个扫描）、FileScanner 元数据解析下放后台线程（ScannedAsset 携带 Asset+Metadata，消除主线程逐文件 readAll）、ImageCache LRU O(1)（std::list + iterator 哈希替代 O(n) removeAll）、generateThumbnail 单次打开文件（移除 m_dimCache）、LightboxWidget COW detach 规避（qAsConst 只读访问）、扫描器新增 CHECK 35（cache-lru-oln-touch）
- 全部构建 0 错 0 警告，35/35 扫描器 PASS，4/4 测试通过（tst_filestorage 已修复：testScanSingleFile 路径分隔符断言错误；新增 4 个 FileWatcher 测试）
- **dcc5b14** — 第 7 轮：GalleryWidget paintEvent 脏区域窄化（`event->rect()` + `paintRegion = dirty.adjusted(-2,-2,2,12)`，覆盖圆角抗锯齿 + hover 阴影下溢 10px；项循环按 `itemRect(i).adjusted(...).intersects(paintRegion)` 跳过无关卡片；背景仅填充 paintRegion）
- 全部构建 0 错 0 警告，34/34 扫描器 PASS，4/4 测试通过
- **第 9 轮（工作区）**：
  - 正确性：收藏图标 `star` 误用 `star-empty` 字形（0xea6a → 0xeb59）；undo "添加标签"会移除操作前已存在的标签（构造时记录 `m_newlyTaggedAssetIds`）；`deleteAssets` commit 失败仍返回 true；CommandPalette 的 undo/redo 不刷新界面、导入文件夹不更新侧栏；点击文件夹后旧标签筛选仍生效；右键菜单作用于旧选区；Ctrl/Shift/Alt 橡皮筋分支不可达；Sidebar 折叠标签区后 `m_addTagRect` 命中区残留；DetailPanel `clear()` 未使异步解码失效
  - 查询/扫描：首屏与懒加载共用 `MainWindow::AssetQuery` + `queryAssets()`（修复"最近导入"tab 翻页排序丢失）；`searchAssets` 追加 `id ASC` 稳定排序；扫描去重从每文件 2 次 SQL 改为 `getAllAssets()` 一次性 path/hash 快照 + 内存集合（`m_scanDepth` / `m_scanTxDepth` 支持并发扫描）；Schema/migrate 失败时初始化直接失败
  - 线程/缓存：QtConcurrent worker 只解码 `QImage`，`QPixmap::fromImage` 统一回到 GUI 线程（DetailPanel/Lightbox/ImageCache）；ImageCache `m_pending` 去重 in-flight 解码、拒绝缓存空 pixmap；Gallery 清空 `m_fileExistsCache`
  - paintEvent：DetailPanel 标签数文本、Gallery 空搜索标题与批量工具栏文本预计算，消除每帧 `.arg()`
  - 扫描器新增 CHECK 36（paintevent-helper-arg-alloc）与 CHECK 37（qtconcurrent-qpixmap-worker）
  - 构建 0 错 0 警告，37/37 扫描器 PASS，5/5 测试通过（新增 tst_librarycontroller：undo 标签/删除恢复/扫描幂等；数据库分页稳定性；异步缩略图）

#### In Progress
- 无

#### Blocked
- 无（tst_filestorage 已在第 6 轮解决：原"静默崩溃"实为 `testScanSingleFile` 的路径分隔符断言错误——`QFileInfo::absoluteFilePath()` 返回正斜杠，测试却用 `QDir::toNativeSeparators()` 比较，已改为 `fromNativeSeparators()`；另新增 computeDiff 3 项 + per-dir 隔离集成测试，4/4 全绿）

### Key Decisions
- 双 SQL 查询（countAssets + searchAssets）用 `COUNT(*) OVER()` 窗口函数合并为单次，减少数据库往返
- `getAllTags()` 缓存到 `MainWindow::m_cachedTags`，仅标签变更时通过 `tagsChanged` 信号刷新
- SDParser 去重读：`detectSource` 和 `parse` 共享同一份 `QByteArray`，消除二次 PNG `readAll()`
- `final` 关键字统一加到所有无子类的具体类，帮助 MSVC devirtualization
- TabBar/TitleBar 的 QFontMetrics 改为成员变量，在构造函数中用字体初始化（fontChanged 信号频率低，无需动态刷新）
- 异步图片加载用 generation 计数器处理竞争（旧 generation 结果丢弃），通过 `QMetaObject::invokeMethod(this, ...)` 回到主线程设置 pixmap + update()
- 惰性 fileExists：删除全部 4 处 `prefetchFileExistence` 调用，改为 paintEvent 中按需 stat + 缓存结果
- FileWatcher 用 `QHash<QString, QStringList> m_knownByDir` 按目录维护已知文件集合，目录间事件隔离（原先全局共享 known 列表导致跨目录误报 fileRemoved → 用户数据被删）
- QFontMetrics 是字体快照（COW）：头文件 `QFontMetrics m_x{m_font}` 初始化后，若构造函数再改 `m_font`，度量不会更新 → 字体改动后必须重新赋值 `m_x = QFontMetrics(m_font)`
- undo/redo 快捷键不再直接连 QUndoStack，改为 lambda 内调用 `m_undoStack->undo()/redo()` 后执行 `reloadAfterUndo()`（loadAssets + 详情面板按 currentAssetId 重拉 getAsset/getMetadata/getTagsForAsset 刷新）
- GalleryWidget::indexAt 由 O(n) 线性扫描改为 O(1) 行列公式计算（`(x-gap)/(itemWidth+gap)` 求列、`(y+scrollOffset-gap)/(itemHeight+gap)` 求行，再回验 itemRect）
- 事务支持嵌套：`beginTransaction` 首次为真事务（`m_db.transaction()`），后续用 `SAVEPOINT pixobook_sp_{depth}`；commit/rollback 按深度递减，最内层用 RELEASE/ROLLBACK TO，外层才真正 COMMIT/ROLLBACK——使扫描长事务与用户批量操作（加标签/删除）并发安全
- 扫描导入整个括在一个事务里（scanStarted begin / scanFinished commit，commit 失败回滚）：10k 文件从 ~30k 次独立 fsync 降为 1 次
- 元数据解析下放后台：`FileScanner::scanDirectorySync` 在后台线程调用 `MetadataParser::parse`，通过 `ScannedAsset{Asset, Metadata}` 携带，主线程只做 DB 写入（消除逐文件 readAll 卡 UI）；ParserRegistry 仅启动时注册 → 后台并发 parse 线程安全
- ImageCache LRU 用 `std::list<CacheKey>` + `std::unordered_map<CacheKey, iterator>`：get() 命中 O(1) splice，替代原 QList removeAll+append 的 O(n)（paint 热路径随缓存规模增长）
- `generateThumbnail` 单个 `QImageReader`：`size()` → `setScaledSize` → `read()` 一次打开，删除冗余 `m_dimCache`
- LightboxWidget 打开不再深拷贝：所有只读访问用 `qAsConst(m_assets)[i]`，仅收藏切换（稀有用户操作）才允许 detach
- 首屏与懒加载统一走 `MainWindow::AssetQuery` + `queryAssets()`：tab 过滤/排序只计算一次，避免 loadMore 复制粘贴逻辑漂移
- `searchAssets` 的 ORDER BY 追加 `id ASC` 作为稳定排序键：相同 `created_at` 的行不会在 LIMIT/OFFSET 分页间重复或漏出
- 扫描去重快照：scanStarted 时用一次 `getAllAssets()` 建立 path/hash 内存集合，assetFound 只查集合；`m_scanDepth` / `m_scanTxDepth` 分离"并发扫描计数"与"已开启事务计数"，最后一个扫描结束才清空集合并提交外层事务
- `QtConcurrent` worker 内只允许 `QImage`：`QPixmap::fromImage` 一律放在 `QMetaObject::invokeMethod` 的 GUI lambda 中执行（DetailPanel/Lightbox/ImageCache）
- ImageCache 的 `m_pending`（unordered_set）防止同一 key 的缩略图被重复后台解码；`insertLocked` 拒绝空 pixmap，避免空值占位后 `get()` 永远 miss
- `AddTagToAssetsCommand` 构造时记录 `m_newlyTaggedAssetIds`（原本没有该标签的 asset），undo 只撤销这些，保护操作前已有的标签关联
- `createSchema()` / `migrate()` 返回 bool：DDL 或 schema_version 写入失败时 `initialize()` 直接失败，避免在空表上静默运行
- 图标字体中 `star` 必须映射 `star-full`（0xeb59），`star-empty` 是 0xea6a；两者混用会让收藏状态视觉上不可区分

### Next Steps
- 新增任何后台解码路径时，worker 只返回 `QImage`，`QPixmap` 必须在 GUI 线程构造（CHECK 37 会拦截）
- 扫描期新增/删除文件的路径必须同步更新 `m_scanKnownPaths` / `m_scanKnownHashes`，否则去重快照会漂移
- 可继续优化扫描的 metadata 写入：目前仍是每文件一次 prepared upsert，后续可批量/复用 QSqlQuery
- 若再发现局部 update(QRect) 但 paintEvent 全量重绘的自绘控件，复用第 7 轮脏区域窄化模式（如 LightboxWidget/SidebarWidget 的热区变化）

### Critical Context
- `QFontMetrics` 构造在 Qt5 中涉及字体引擎初始化，在 paintEvent 循环中被多次调用时产生可测量的每帧分配
- 异步图片加载需要 `#include <QtConcurrent>` + `#include <QPointer>` 在 .cpp 中，`m_loadGeneration` 在 .h 中声明为 `int`，以 0 初始化
- `QImageReader::read()` 对高分辨率图片（>4K）可阻塞主线程 200-500ms，离线解码后 `invokeMethod` 回主线程设置 QPixmap + update()
- `QPixmap` 绑定 GUI 线程，工作线程只能解码 `QImage`；跨线程返回 `QPixmap` 在部分平台/驱动上会崩溃或渲染异常
- ImageCache `finishThumbnail` 必须在同一把 `m_mutex` 下先 erase `m_pending` 再 `insertLocked`，emit 放到锁外
- 扫描快照期间 FileWatcher 的 fileAdded/fileRemoved 必须同步维护集合；并发扫描通过 `m_scanDepth` / `m_scanTxDepth` 配对提交
- `DELETE`/写操作失败后 `rollbackTransaction()` 也可能失败，调用方应以操作返回值为准，不能假定回滚成功

### Relevant Files
- `src/ui/TabBar.h/.cpp`：新增 `QFontMetrics m_tabFontMetrics{m_tabFont}` 成员，`tabRect`/`addButtonRect` 移除临时 QFontMetrics
- `src/ui/TitleBar.h/.cpp`：新增 `QFontMetrics m_titleFontMetrics{m_titleFont}` 成员，`menuItemRect` 移除临时 QFontMetrics
- `src/ui/DetailPanel.h/.cpp`：新增 `int m_loadGeneration`、`<QtConcurrent>` + `<QPointer>`、`showAsset` 异步图片加载、`clampScrollOffset` 移出 paintEvent、5 个 QFontMetrics 成员 + `m_zoomText` 预计算
- `src/ui/LightboxWidget.h/.cpp`：新增 `int m_loadGeneration`、`<QtConcurrent>` + `<QPointer>`、`loadCurrentImage` 异步图片加载
- `src/ui/GalleryWidget.h/.cpp`：删除 `prefetchFileExistence` 声明/实现/全部 4 处调用、paintEvent 中按需 stat + 缓存、新增 `m_badgeFontFm`、`drawEmptyState` 静态 QVector
- `src/ui/SidebarWidget.h/.cpp`：新增 `QFontMetrics m_sectionFontFm` / `m_itemFontFm`、`QFileInfo` → 纯字符串操作
- `src/services/FileScanner.cpp`：`QFileInfo` → 纯字符串操作（suffix 提取）
- `src/services/FileWatcher.h/.cpp`：全局 `m_knownFiles` → `QHash<QString, QStringList> m_knownByDir` 按目录隔离、新增静态 `FileDiff computeDiff`（含输出去重）、unwatch 剪枝
- `src/ui/MainWindow.h/.cpp`：undo/redo 快捷键 → `reloadAfterUndo()`（loadAssets + 详情面板按 currentAssetId 重拉刷新）
- `src/ui/TabBar.cpp`/`TitleBar.cpp`/`DetailPanel.cpp`/`SidebarWidget.cpp`：构造函数 setPixelSize 后重新赋值 QFontMetrics 成员（修复头文件初始化快照失效）
- `src/services/LibraryController.cpp`：`addTagToAssets`/`deleteAssets` commit 失败时 rollback
- `src/ui/GalleryWidget.cpp`：`indexAt` 线性扫描 → O(1) 行列公式计算；paintEvent 脏区域窄化（`event->rect()` + `paintRegion.adjusted(-2,-2,2,12)` + 项跳过测试）
- `tests/tst_filestorage.cpp`：`testScanSingleFile` 路径断言 `toNativeSeparators` → `fromNativeSeparators`；新增 computeDiff 3 项 + per-dir 隔离集成测试
- `src/database/DatabaseManager.h/.cpp`：事务 SAVEPOINT 嵌套（`m_txDepth` + `pixobook_sp_{depth}`），启用扫描长事务且与批量操作并发安全
- `src/services/FileScanner.h/.cpp`：`scanStarted` 信号、`ScannedAsset{Asset, Metadata}`、后台元数据解析下放、`Q_DECLARE_METATYPE(ScannedAsset)`
- `src/services/LibraryController.cpp`：scanStarted begin / assetFound 只写 DB / scanFinished commit（失败回滚）
- `src/services/ImageCache.h/.cpp`：LRU `std::list` + `std::unordered_map<iterator>`（O(1) splice）、`generateThumbnail` 单次打开、移除 `m_dimCache`
- `src/ui/LightboxWidget.cpp`：3 处只读访问改 `qAsConst(m_assets)[i]` 消除打开深拷贝
- `src/ui/Codicon.h`：修正 `star` → 0xeb59（star-full），新增 `star-full` 别名
- `src/services/LibraryController.h/.cpp`：扫描 path/hash 内存快照、`m_scanDepth`/`m_scanTxDepth` 并发事务计数、`deleteAssets` commit 失败返回 false、插入失败日志
- `src/ui/UndoCommands.h/.cpp`：`AddTagToAssetsCommand` 记录 `m_newlyTaggedAssetIds`，undo 只撤销新增关联
- `src/ui/MainWindow.h/.cpp`：`AssetQuery`/`queryAssets()` 统一查询、命令面板 undo/redo 刷新与导入文件夹、folderSelected 清除标签筛选
- `src/ui/GalleryWidget.h/.cpp`：右键先选当前项、空白区橡皮筋支持修饰键、`m_emptySearchTitle`/`m_batchSummaryText` 预计算、setAssets 清空 fileExists 缓存
- `src/ui/SidebarWidget.cpp`：折叠标签区清空 `m_addTagRect`（鼠标/键盘）
- `src/ui/DetailPanel.h/.cpp`：`clear()` 递增 `m_loadGeneration`、`m_favStarRect` 复位、`m_tagCountText` 预计算、worker 仅解码 QImage
- `src/ui/LightboxWidget.cpp`：worker 仅解码 QImage，GUI 线程 `QPixmap::fromImage`
- `src/services/ImageCache.h/.cpp`：`m_pending` in-flight 去重、`insertLocked` 拒绝空 pixmap、`renderThumbnail` 返回 QImage
- `src/database/DatabaseManager.h/.cpp`：`createSchema`/`migrate` 返回 bool、稳定排序 `id ASC`、析构时先释放连接句柄
- `tests/tst_librarycontroller.cpp`：新增 undo 标签/删除恢复/扫描幂等测试
- `tests/tst_database.cpp`：新增相同时间戳分页稳定性测试
- `tests/tst_filestorage.cpp`：新增空 pixmap 不缓存、异步缩略图测试；initTestCase 校验临时目录
- `ci/scan-bug-patterns.ps1`：37 个检查块，自动扫描（新增 CHECK 36/37）

---

## 前置检查清单（每次修改代码前）

- [ ] 已加载 `clang-format` 配置文件（`.clang-format`）
- [ ] 已检查 `.clang-tidy` 中启用的检查项
- [ ] 已知悉 `ci/scan-bug-patterns.ps1` 中所有检查规则

## 修改代码后

1. **运行扫描脚本**：
   ```powershell
   powershell -ExecutionPolicy Bypass -File ci/scan-bug-patterns.ps1
   ```

2. **如果安装了 LLVM/clang**，也运行：
   ```powershell
   cmake --build build --target check-format
   cmake --build build --target run-tidy
   ```

3. **构建验证**：
   ```powershell
   cmake --build build --config RelWithDebInfo -j
   ctest -C RelWithDebInfo --output-on-failure
   ```

## Bug 模式检查清单（Code Review 用）

每项对应一个已知曾在代码库中实际发生的 bug：

| # | 模式 | 危害 | 检查方法 |
|---|------|------|----------|
| 1 | 析构函数未 delete 裸指针成员 | 内存泄漏 | 检查 `.h` 中 `Type *m_` 成员，对应 `.cpp` 的析构函数是否有 `delete` |
| 2 | 乘法后截断到 32-bit | 数据溢出/缩略图错乱 | 检查 `(int)(a * b)` 或 `(quint32)(a * b)`，尤其宽×高×字节 |
| 3 | SQL 字符串拼接 | SQL 注入 | 检查 SQL 查询是否用 `+` 或 `.arg()` 拼接用户输入 |
| 4 | 声明但从未连接的 slot/函数 | 死代码 | 检查 `.h` 中声明的函数在 `.cpp` 中是否有 `connect` 或直接调用 |
| 5 | `QtConcurrent::run` 捕获 `[this]` | 对象销毁后仍执行 lambda，UAF | 使用 `QPointer<T> guard(this)` + `[\guard]` 捕获替代 `[this]` |
| 6 | `qUncompress` 结果未判空 | 空 QByteArray 导致后续崩溃 | 检查 `qUncompress` 后是否有 `isEmpty()` 保护 |
| 7 | `exec()` 返回值未检查 | PRAGMA/事务静默失败 | 检查 `.*exec\(.*\)` 是否被 if 包裹或返回值被检查 |
| 8 | `setScaledSize` + `IgnoreAspectRatio` | 图片拉伸变形 | 检查 `setScaledSize` 附近是否有 `KeepAspectRatio` |
| 9 | `clear()` 未重置累加器 | `m_currentBytes` 等计数持续累积失效 | 检查 `clear()` 是否同时清零所有计数成员 |
| 10 | `QPainterPath` / `QFont` / `QString::arg()` 在 paintEvent 中 | 每帧堆分配，卡顿 | paintEvent 中的对象创建应改为成员变量缓存 |
| 11 | 裸 `QObject*` 指针非 `QPointer` | 野指针 use-after-free | 检查非父子关系的 `QObject*` 成员应使用 `QPointer` |
| 14 | 辅助绘制方法未标记 `const` | 编译期无法捕获非法修改 | 检查 `draw*` 辅助方法的 `.h` 定义和 `.cpp` 实现是否都加了 `const` |
| 17 | 跨线程信号的参数类型未注册 Q_DECLARE_METATYPE | 跨线程信号静默失败 | 检查信号参数中的自定义类型是否有 `Q_DECLARE_METATYPE` |
| 18 | C-style cast 未替换为 static_cast | 隐式转换可能丢失精度，不符合 C++17 规范 | 检查 `(int)`, `(double)` 等 C 风格转型 |
| 12 | Windows 路径比较未忽略大小写 | 路径匹配失败 | 检查 `.startsWith(path)` / `==` 是否使用 `Qt::CaseInsensitive` |
| 13 | 事务没有回滚路径 | 部分执行后数据库不一致 | 检查 `beginTransaction` 后，分支路径是否都有 `rollback` |
| 19 | `QDateTime::fromString` 未验证 `isValid()` | 无声的数据损坏 | 检查 `fromString` 结果的 `isValid()` 调用 |
| 20 | Range-for 遍历成员容器导致隐式 detach | 每帧不必要的写时复制开销 | 在非 const 方法中遍历 `m_` 容器时使用 `qAsConst()` |
| 21 | paintEvent 辅助方法中的 blocking I/O 或堆分配 | 每帧文件系统访问 + 堆分配卡顿 | 检查 `draw*` 方法中是否有 `QFileInfo`、`QImageReader`、`.arg()`、`QString::number()` |
| 22 | Range-for 显式类型遍历成员容器未用 `qAsConst` | 写时复制隐式 detach | 检查 `for (int x : m_container)` 模式是否在非 const 方法中 |
| 23 | `QtConcurrent::run` lambda 内直接 `emit` 信号 | 跨线程信号发射，新增直接连接接收者时可能崩溃 | 应使用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 封装 |
| 24 | `QFile` 打开后未显式 `close()` | 文件描述符未及时释放（长循环中尤为明显） | 检查 `QFile::open()` 后是否有匹配的 `close()` |
| 25 | 大型容器（`QVector<Asset>`）通过值返回 | 不必要的深拷贝，大图库场景性能损耗 | 检查返回 `QVector<T>` 的方法是否仅包装成员，改为返回 `const &` |
| 26 | paintEvent 中冗余 `QFileInfo(…).fileName()` | 每帧不必要的文件系统访问 | 检查 paintEvent 中是否直接用 `QFileInfo` 提取文件名，改用预先存储的值 |
| 27 | 匿名命名空间中的堆分配函数被 paintEvent 调用链间接使用 | 每帧 QStringList + .arg() 堆分配 | 检查匿名命名空间中包含 `.arg()`/`QString::number()` 的函数是否会被 paintEvent 路径调用 |
| 28 | 缓存 `get()` 未更新 `m_accessOrder` | 缓存实际行为为 FIFO 而非 LRU，导致高频访问项被错误驱逐 | 检查缓存类的 `get()`/`find()` 方法中是否在命中后更新了访问顺序 |
| 29 | `pixmapBytes()` 无空 pixmap 守卫 | 空 QPixmap 的 depth() 返回 0，除 8 后为 0，导致缓存大小跟踪失真 | 检查 `depth() / N` 表达式附近是否有 `isNull()` 保护 |
| 30 | return 前冗余的 `close()` 调用 | 背离 RAII 异常安全原则，若函数抛出异常则文件描述符泄漏 | 检查 `close()` 是否后接 `return`，此时应依赖 RAII |
| 31 | paintEvent 中的 `.trimmed()` 调用 | 每帧堆分配 | 检查 paintEvent 或 `draw*` 辅助方法中是否直接调用 `.trimmed()` |
| 32 | `QString` 值传递参数 | 不必要的 COW 原子操作 | 检查 `.h` 中函数参数是否为 `QString` (非 `const &`) |
| 33 | `QFileInfo` 仅用于字符串操作 | 不必要的文件系统 stat 调用 | 检查 `QFileInfo(…).fileName()` / `.suffix()` 等纯字符串操作 |
| 34 | 头文件 `QFontMetrics m_x{m_font}` 初始化后构造函数改 `m_font` 未重建 | 度量快照失效（字体改了但测量不更新） | 检查 QFontMetrics 成员对应的 QFont 在 .cpp 中是否有 `setPixelSize`/赋值，且之后未重新赋值 `m_x = QFontMetrics(...)` |
| 35 | 图标字体 codepoint 映射错误（`star` 用了 `star-empty` 的 0xea6a） | 收藏/非收藏状态视觉上完全相同 | 对照 `resources/codicon.css` 的 `.codicon-<name>:before` 校验 `Codicon::icon()` 映射 |
| 36 | `QtConcurrent::run` 工作线程内创建 `QPixmap` / `QPainter` | QPixmap 绑定 GUI 线程，可能崩溃或渲染异常 | worker 只解码 `QImage`；`QPixmap::fromImage` 放在 `QMetaObject::invokeMethod` 的 GUI lambda 中 |
| 37 | 缓存 `insert()` 存入空 pixmap / 同一 key 并发重复解码 | 空值永久占位（get 永远 miss）+ 每帧重复派发解码 | `insert` 拒绝 null；用 `m_pending` 集合去重 in-flight 请求 |
| 38 | 带修饰键的鼠标操作分支位于"空白区"分支之前 | 修饰键+橡皮筋等组合功能不可达（死代码） | 检查 `modifiers()` 分支是否先于 `idx < 0` / 空白区处理 |

## 已修复的实例

| Bug 模式 | 修复文件 | 修复内容 |
|----------|---------|----------|
| QtConcurrent UAF (模式 5) | `FileScanner.cpp`, `ImageCache.cpp` | `[this]` → `QPointer` guard |
| 路径大小写 (模式 12) | `FileWatcher.cpp` | `startsWith` 添加 `Qt::CaseInsensitive` |
| 死 slot (模式 4) | `FileWatcher.h` | 移除未使用的 `fileModified(QString)` 信号 |
| paintEvent 堆分配 (模式 10) | GalleryWidget, DetailPanel, TabBar, TitleBar, SidebarWidget, LightboxWidget, SearchBar | QPainterPath → drawRoundedRect; QFont → 成员变量缓存; QString::arg → 预计算 |
| 跨线程信号未注册 (模式 17) | Asset.h, main.cpp | 添加 Q_DECLARE_METATYPE(Asset) + qRegisterMetaType<Asset>() |
| C-style cast (模式 18) | LightboxWidget, DetailPanel, CustomStyle | `(int)expr` → `static_cast<int>(expr)` |
| QPainterPath 未使用 include | DetailPanel, TitleBar, LightboxWidget, TabBar, SearchBar | 移除多余的 `#include <QPainterPath>` |
| 辅助绘制方法未标记 `const` (模式 14) | GalleryWidget, DetailPanel | `drawEmptyState`, `drawRubberBand`, `drawSectionDivider`, `drawSummaryAction`, `drawField`, `drawScrollIndicator` → 全部加 `const` |
| QDateTime::fromString 未验证 (模式 19) | DatabaseManager.cpp | 添加 `parseIsoDt` 包装函数，10 处调用全部增加 `isValid()` 保护 |
| Range-for 隐式 detach (模式 20) | FileWatcher.cpp, DetailPanel.cpp (x2), GalleryWidget.cpp | 为遍历 `m_` 容器的 range-for 添加 `qAsConst()` 包装 |
| paintEvent blocking I/O (模式 21) | DetailPanel.cpp | 将 `QFileInfo`、`QString::number()`、`.arg()` 移出 draw* 方法，在 `showAsset()` 中预计算为成员变量 |
| Range-for 显式类型 (模式 22) | GalleryWidget.cpp | `for (int idx : m_selectedIndices)` → `qAsConst(m_selectedIndices)` |
| QtConcurrent 跨线程 emit (模式 23) | ImageCache.cpp, FileScanner.cpp | 将 `QtConcurrent::run` lambda 中的直接 `emit` 改为 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` |
| QFile 未显式 close (模式 24) | ParserRegistry.cpp | 移除冗余的显式 `close()`，统一使用 RAII 管理文件生命周期 |
| 大型容器返回值 (模式 25) | GalleryWidget.h/.cpp, MainWindow.cpp | `allAssets()` 返回类型从 `QVector<Asset>` 改为 `const QVector<Asset>&` |
| paintEvent QFileInfo (模式 26) | LightboxWidget.cpp, SidebarWidget.h/.cpp | `QFileInfo(asset.filePath).fileName()` → `asset.fileName`；新增 `m_folderDisplayNames` 预计算文件夹显示名 |
| 匿名命名空间堆分配 (模式 27) | DetailPanel.cpp | 删除 `detailMetaLine()` 自由函数，在 `showAsset()` 中预计算 `m_metaLine` |
| 缓存访问顺序 (模式 28) | ImageCache.cpp | `get()` 方法中添加 `m_accessOrder` 更新，使缓存行为由 FIFO 变为 LRU |
| pixmapBytes 空守卫 (模式 29) | ImageCache.h | `pixmapBytes()` 添加 `isNull()` 守卫，防止空 pixmap 的 depth() 为 0 |
| 冗余 close (模式 30) | FileScanner.cpp, ParserRegistry.cpp | 移除显式 `close()` 调用，重建 RAII 异常安全 |
| CacheKey 短路比较 | ImageCache.h | `operator==` 中 `size` 先于 `filePath` 比较（int 短路 QString） |
| renameTag SQL 化 | IDatabaseManager.h, DatabaseManager.h/.cpp, LibraryController.cpp | 新增 `getTag(id)` 替代 `getAllTags()` 全表扫描 |
| qPrintable 替换 | DatabaseManager.cpp | `.toUtf8().constData()` → `qPrintable()` |
| 跨目录误删数据丢失 | FileWatcher.h/.cpp | 全局 `m_knownFiles` → `QHash<QString, QStringList> m_knownByDir` 按目录维护已知集合，checkForNewFiles 只 diff 自身目录；新增静态 `computeDiff` 含输出去重 |
| QFontMetrics 快照失效 (模式 34) | TabBar.cpp, TitleBar.cpp, DetailPanel.cpp, SidebarWidget.cpp | 构造函数 `setPixelSize` 后重新赋值 QFontMetrics 成员（如 `m_tabFontMetrics = QFontMetrics(m_tabFont)`） |
| undo/redo 不刷新界面 | MainWindow.h/.cpp | undo/redo 快捷键改为 lambda 内调用 `m_undoStack->undo()/redo()` 后执行 `reloadAfterUndo()` |
| 事务提交失败不回滚 (模式 13) | LibraryController.cpp | `addTagToAssets`/`deleteAssets` commit 失败时调用 `rollbackTransaction()` |
| indexAt O(n) | GalleryWidget.cpp | `itemRect` 线性扫描 → O(1) 行列公式计算并回验 |
| 路径分隔符断言错误 | tests/tst_filestorage.cpp | `QCOMPARE(a.filePath, QDir::toNativeSeparators(path))` → `fromNativeSeparators`（QFileInfo::absoluteFilePath 返回正斜杠） |
| 局部 update 全量重绘 | GalleryWidget.cpp | paintEvent 改用 `event->rect()`：`paintRegion = dirty.adjusted(-2,-2,2,12)` 只填充/重绘受影响区域，项按 `itemRect.adjusted(...).intersects(paintRegion)` 跳过；hover/橡皮筋/滚动条拖拽时从全量 ~30 卡片降为 0~4 张 |
| 扫描导入逐语句 fsync | DatabaseManager.cpp, LibraryController.cpp | `beginTransaction` 支持 SAVEPOINT 嵌套；scanStarted begin / scanFinished commit（失败回滚）把整个扫描括进一个事务，10k 文件从 ~30k 次 commit 降为 1 次 |
| 主线程逐文件元数据解析 | FileScanner.cpp, LibraryController.cpp | 解析下放后台线程：`ScannedAsset{Asset, Metadata}` 携带，`assetFound` 信号改发 ScannedAsset；主线程只做 DB 写入 |
| LRU O(n) touch (模式 28 增强) | ImageCache.h/.cpp | `QList<CacheKey> m_accessOrder` + `removeAll` → `std::list` + `std::unordered_map<CacheKey, iterator>`，get() 命中 O(1) splice |
| 缩略图双次打开文件 | ImageCache.cpp | 探测 `QImageReader probe` + 读取 `QImageReader reader` 合并为单个 `QImageReader`（size→setScaledSize→read），删除 `m_dimCache` |
| 灯箱打开深拷贝 | LightboxWidget.cpp | `m_assets[m_currentIndex]` → `qAsConst(m_assets)[m_currentIndex]`（3 处只读访问），打开 2000 项图库从 ~5 万次堆分配降为 0，仅收藏切换才 detach |
| 跨线程信号类型未注册 (模式 17) | FileScanner.h | `ScannedAsset` 信号参数补 `Q_DECLARE_METATYPE(ScannedAsset)` |
| 扫描事务无回滚 (模式 13) | LibraryController.cpp | scanFinished 中 commit 失败时 `rollbackTransaction()` |
| 收藏图标字形错误 (模式 35) | Codicon.h | `star` 从 0xea6a（star-empty）改为 0xeb59（star-full），并新增 `star-full` 别名 |
| undo 误删既有标签 | UndoCommands.h/.cpp | `AddTagToAssetsCommand` 构造时记录 `m_newlyTaggedAssetIds`，undo 只移除命令新增的关联 |
| commit 失败仍返回成功 (模式 13) | LibraryController.cpp | `deleteAssets` commit 失败时 rollback、emit dataChanged、返回 false |
| CommandPalette 撤销不刷新 | MainWindow.cpp | 命令面板 undo/redo 与快捷键一致，执行后调用 `reloadAfterUndo()` |
| 命令面板导入文件夹不更新侧栏 | MainWindow.cpp | 改为调用 `onImportFolder()`（scanFolder + setFolders） |
| 文件夹选择残留标签筛选 | MainWindow.cpp | `folderSelected` 中 `m_activeTagId = -1` |
| 右键菜单操作旧选区 | GalleryWidget.cpp | 右键未选中项时先选中该项并 emit assetSelected |
| 修饰键橡皮筋不可达 (模式 38) | GalleryWidget.cpp | 将 `idx < 0` 空白区分支前移，再处理 Ctrl/Shift/Alt |
| 折叠区残留命中框 | SidebarWidget.cpp | 折叠标签区时清空 `m_addTagRect` 与 hover 状态（鼠标与键盘路径） |
| 异步加载越过 clear | DetailPanel.cpp | `clear()` 中 `++m_loadGeneration`，丢弃旧解码结果；同时重置 `m_favStarRect` |
| 分页过滤器漂移 | MainWindow.h/.cpp | `AssetQuery` + `queryAssets()` 统一首屏与 loadMore 的筛选/排序 |
| 相同时间戳分页重复/漏项 | DatabaseManager.cpp | `searchAssets` ORDER BY 追加 `id ASC` 稳定排序键 |
| 扫描逐文件 2 次去重查询 | LibraryController.h/.cpp | scanStarted 快照 path/hash 集合 + 内存去重；`m_scanDepth`/`m_scanTxDepth` 支持并发扫描 |
| 工作线程创建 QPixmap (模式 36) | DetailPanel.cpp, LightboxWidget.cpp, ImageCache.h/.cpp | worker 只解码 QImage；`QPixmap::fromImage` 在 GUI 线程 lambda 内执行 |
| 空 pixmap 占位 + 重复解码 (模式 37) | ImageCache.h/.cpp | `insertLocked` 拒绝 null；`m_pending` 去重 in-flight 解码 |
| Schema DDL 静默失败 (模式 7) | DatabaseManager.h/.cpp | `createSchema`/`migrate` 返回 bool，任一步失败则 `initialize()` 失败 |
| 数据库连接析构告警 | DatabaseManager.cpp | close 后 `m_db = QSqlDatabase()` 再 removeDatabase，避免连接仍在使用告警 |
| paintEvent `.arg()` 漏检 | GalleryWidget.h/.cpp, DetailPanel.h/.cpp | 预计算 `m_emptySearchTitle` / `m_batchSummaryText` / `m_tagCountText` |

## 扫描器改进

| 改进 | 说明 |
|------|------|
| CHECK 1 增强 | QObject 类 + `= default` 析构函数跳过检查（依赖注入模式） |
| CHECK 1 增强 | 检查成员类型在 .cpp 中是否被 `new` 分配，仅标记实际拥有的裸指针 |
| CHECK 5 精确化 | 只检查 `QtConcurrent::run([this])` 中的 `[this]` 捕获，排除 `QPointer guard(this)` 上下文 |
| CHECK 14 精确化 | 通过函数名回溯判断 `.arg()` 是否真正在 paintEvent 函数体内，消除跨函数误报 |
| CHECK 14 增强（模式 14） | 辅助绘制方法未标记 `const` — 新增检查项并添加 scanner 规则 |
| CHECK 17（新增） | 检测信号参数中缺少 Q_DECLARE_METATYPE 的自定义类型 — 自动扫描所有 Q_OBJECT 类的信号签名 |
| CHECK 18（新增） | 检测 C-style cast `(int)`, `(double)` 等 — 自动扫描所有 .cpp 文件 |
| CHECK 19（新增） | 检测 `QDateTime::fromString` 结果未调用 `isValid()` — 无声数据损坏 |
| CHECK 20（新增） | 检测 range-for 遍历成员容器时未使用 `qAsConst()` — 隐式 detach 性能损耗 |
| CHECK 21（新增） | 检测 paintEvent 辅助方法中的 blocking I/O（`QFileInfo`、`QImageReader`）和堆分配（`.arg()`、`QString::number()`）— 通过 `Get-NearestFunctionName` 追踪调用链 |
| CHECK 22（新增） | 检测 range-for 中使用显式类型（非 `auto`）遍历 `m_` 成员容器未加 `qAsConst()` — 补充 CHECK 20 的正则覆盖 |
| CHECK 23（新增） | 检测 `QtConcurrent::run` lambda 内的直接 `emit` 信号 — 自动排除已包装在 `QMetaObject::invokeMethod` 中的安全 emit |
| CHECK 24 增强 | 精细化：仅检测非局部 QFile（成员/静态/指针变量）缺少显式 close()；局部 RAII 变量不检测 |
| CHECK 25（新增） | 检测非抽象类中大型容器（`QVector<Asset>` 等）通过值返回 — 仅标记实现仅为 `return m_member;` 的方法 |
| CHECK 26（新增） | 检测 paintEvent 路径中的冗余 `QFileInfo(…).fileName()` — 每帧不必要的文件系统访问 |
| CHECK 27（新增） | 检测匿名命名空间中包含 `.arg()`/`QString::number()` 堆分配的函数 — 排除预计算模式（存储到成员/容器） |
| CHECK 28（新增） | 检测缓存类的 `get()`/`find()` 方法中未更新访问顺序 — 防止缓存退化为 FIFO |
| CHECK 29（新增） | 检测 `depth() / N` 表达式附近缺少 `isNull()` 守卫 — 空 QPixmap 导致缓存大小跟踪失真 |
| CHECK 30（新增） | 检测 `close()` 后紧跟 `return` 的反模式 — 背离 RAII 异常安全原则 |
| CHECK 31（新增） | 检测 paintEvent 路径中的 `.trimmed()` 调用 — 每帧堆分配 |
| CHECK 32（新增） | 检测 `QString` 按值传递参数 — 应使用 `const QString&` |
| CHECK 33（新增） | 检测 `QFileInfo` 仅用于字符串操作（`fileName()` / `suffix()` 等纯字符串方法）— 使用字符串操作替代 |
| CHECK 32 修正 | 正则仅匹配参数位置（`(QString name` / `, QString name`，含 `const QString name`），排除函数返回类型误报；接受 `const QString &` 与 `const QString&` 两种拼写 |
| CHECK 34（新增） | 检测头文件 `QFontMetrics m_x{m_font}` 初始化但 .cpp 构造函数对 `m_font` 调用 `setPixelSize`/赋值后未重建 metrics — 防止 QFontMetrics 快照失效回归 |
| CHECK 35（新增） | 检测 `get()`/`find()` 方法内的 `.removeAll()` — O(n) LRU touch，paint 热路径随缓存规模增长；应使用 `std::list` + iterator 哈希 O(1) splice |
| CHECK 36（新增） | 检测 paintEvent 或 `draw*` 辅助方法内的 `.arg()` — 用"最近的前置函数定义行"回溯，避免被上一行的 `Namespace::call()` 误导（CHECK 21 的 `Get-NearestFunctionName` 存在该盲区） |
| CHECK 37（新增） | 检测 `QtConcurrent::run` 工作线程段内的 `QPixmap` / `QPainter` / `generateThumbnail()` — 在 `QMetaObject::invokeMethod` 处截断以排除 GUI lambda，并剥离 `//` 注释避免文档字符串误报 |

## 递归自我改进流程

```
发现新 bug 类型
  → 添加到 ci/scan-bug-patterns.ps1（新 CHECK 块）
  → 更新本文件（Bug 模式检查清单 + 已修复实例）
  → 更新本文件（扫描器改进）
  → 下次自动化扫描自动检出
```

### 添加新检查的步骤

1. 在 `ci/scan-bug-patterns.ps1` 末尾添加新的 `# CHECK N:` 块
2. 使用 `Search-Files` 和 `Check-Pattern` 辅助函数
3. 结果对象必须有 `Check`、`Status`、`Details` 三个字段
4. 更新本清单表格，增加一行
5. 更新「已修复的实例」部分
6. 运行 `ci/scan-bug-patterns.ps1` 验证新检查正常工作

## 参考

- `.clang-tidy` — clang-analyzer-*, bugprone-*, performance-*, cert-* 系列检查
- `.clang-format` — 4 空格缩进、Allman 函数大括号、m_ 前缀风格
- `ci/scan-bug-patterns.ps1` — 零依赖的完整自动化扫描
