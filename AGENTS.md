# PixoBook — 递归自我改进

本文件指导 opencode agent 在代码变更后自动进行静态分析和质量检查。

---

## Anchored Summary

### Goal
- 对 PixoBook 项目进行多轮 C++ 性能优化评审与修复，涵盖数据库查询、I/O、paintEvent 堆分配、虚函数去虚化、数据局部性

### Constraints & Preferences
- 使用 C++17 + Qt5，需兼容 MSVC 编译器
- 扫描器 ci/scan-bug-patterns.ps1 是零依赖 PowerShell 脚本，全部 33 个检查块在其中可用
- 源码修复必须在 src/ 目录下，遵循现有代码风格（m_ 前缀成员、Allman 大括号）
- 当前处于实施模式，已提交 3 个 perf commit (ddd08ee, 7d322af, bda1aeb)，本轮修复中

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

#### In Progress
- 无

#### Blocked
- tst_filestorage 静默崩溃（预存问题，HEAD~1 已存在），与本次变更无关

### Key Decisions
- 双 SQL 查询（countAssets + searchAssets）用 `COUNT(*) OVER()` 窗口函数合并为单次，减少数据库往返
- `getAllTags()` 缓存到 `MainWindow::m_cachedTags`，仅标签变更时通过 `tagsChanged` 信号刷新
- SDParser 去重读：`detectSource` 和 `parse` 共享同一份 `QByteArray`，消除二次 PNG `readAll()`
- `final` 关键字统一加到所有无子类的具体类，帮助 MSVC devirtualization
- TabBar/TitleBar 的 QFontMetrics 改为成员变量，在构造函数中用字体初始化（fontChanged 信号频率低，无需动态刷新）
- 异步图片加载用 generation 计数器处理竞争（旧 generation 结果丢弃），通过 `QMetaObject::invokeMethod(this, ...)` 回到主线程设置 pixmap + update()
- 惰性 fileExists：删除全部 4 处 `prefetchFileExistence` 调用，改为 paintEvent 中按需 stat + 缓存结果

### Next Steps
- GalleryWidget paintEvent 使用 `event->rect()` 缩小脏区域（低优先级，密集布局下收益有限）
- 考虑 TiMidity 子进程的 stdio 读取超时（已有 `QTimer::singleShot(5000, ...)`，可在更复杂场景增加）

### Critical Context
- `QFontMetrics` 构造在 Qt5 中涉及字体引擎初始化，在 paintEvent 循环中被多次调用时产生可测量的每帧分配
- 异步图片加载需要 `#include <QtConcurrent>` + `#include <QPointer>` 在 .cpp 中，`m_loadGeneration` 在 .h 中声明为 `int`，以 0 初始化
- `QImageReader::read()` 对高分辨率图片（>4K）可阻塞主线程 200-500ms，离线解码后 `invokeMethod` 回主线程设置 QPixmap + update()
- `prefetchFileExistence` 对所有 asset（含不可见项）执行 stat，大规模图库（>10000 项）场景产生大量不必要的文件系统调用
- tst_filestorage 静默崩溃是预存问题，与本次变更无关

### Relevant Files
- `src/ui/TabBar.h/.cpp`：新增 `QFontMetrics m_tabFontMetrics{m_tabFont}` 成员，`tabRect`/`addButtonRect` 移除临时 QFontMetrics
- `src/ui/TitleBar.h/.cpp`：新增 `QFontMetrics m_titleFontMetrics{m_titleFont}` 成员，`menuItemRect` 移除临时 QFontMetrics
- `src/ui/DetailPanel.h/.cpp`：新增 `int m_loadGeneration`、`<QtConcurrent>` + `<QPointer>`、`showAsset` 异步图片加载、`clampScrollOffset` 移出 paintEvent
- `src/ui/LightboxWidget.h/.cpp`：新增 `int m_loadGeneration`、`<QtConcurrent>` + `<QPointer>`、`loadCurrentImage` 异步图片加载
- `src/ui/GalleryWidget.h/.cpp`：删除 `prefetchFileExistence` 声明/实现/全部 4 处调用、paintEvent 中按需 stat + 缓存
- `ci/scan-bug-patterns.ps1`：33 个检查块，自动扫描

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
