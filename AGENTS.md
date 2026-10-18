# PixoBook — 递归自我改进

本文件指导 opencode agent 在代码变更后自动进行静态分析和质量检查。

---

## Anchored Summary

### Goal
- 对 PixoBook 项目进行多轮 C++ 性能优化评审与修复，涵盖数据库查询、I/O、paintEvent 堆分配、虚函数去虚化、数据局部性
- 第 11 轮起转向第一性原理：先保数据（派生数据 vs 原生注释）、再立度量（benchmark）、后改架构（检索/抽取）

### Constraints & Preferences
- 使用 C++17 + Qt5，需兼容 MSVC 编译器
- 扫描器 ci/scan-bug-patterns.ps1 是零依赖 PowerShell 脚本，全部 39 个检查块在其中可用
- 源码修复必须在 src/ 目录下，遵循现有代码风格（m_ 前缀成员、Allman 大括号）
- 第 8 轮及以前 8 个 commit；第 9 轮 c109f98 / 8f10210 / 1f1e835；第 10 轮 b2275b4 / a771806 / 3b2066c；滚动条 cdc4996 / 7568863；审计修复 26d9a8f；工作区干净

### Progress

#### Done
- 第 1-8 轮：paintEvent 零分配、QFontMetrics 缓存、异步图片解码、脏区域重绘、LRU O(1)、扫描事务批处理、元数据解析下放、FileWatcher 目录隔离。详见 git log 与 Key Decisions。
- 第 9 轮（c109f98/8f10210/1f1e835）：收藏图标、undo 标签、右键选区、异步代际、查询统一、扫描去重快照等 20+ 正确性修复。
- 第 10 轮（b2275b4/a771806/3b2066c）：文件夹前缀筛选、refreshAssets 保选择、已知路径跳过、新子目录补扫；tst_database 每用例独立库；37/37 扫描器、6/6 测试。
- **第 11 轮（工作区）— P0 数据生命周期 + 基准**：
  - 数据分层：`assets` 新增 `missing_since`（schema v3 + 部分索引）。`FileWatcher::fileRemoved` 不再 `deleteAsset`，改为标记缺失；条目、标签、收藏全部保留。删除只由用户显式触发（清理缺失记录/删除素材）。
  - 恢复路径：`fileAdded`/手动导入/文件夹重扫都会恢复同一行。`scanFolder` 的 `knownPaths` 只包含在线条目，缺失路径重新进入扫描器；`assetFound` 先查 `m_scanMissingAssets` 再查 known 集合，避免被当作"已导入"跳过；同 hash 改名由 `adoptRenamedAsset` 清除缺失标记。
  - 迁移安全：`migrate()` 在升级前执行 `wal_checkpoint(FULL)` 并复制为 `*.bak-v{n}`；v3 的 `ALTER TABLE` 先用 `PRAGMA table_info` 判重，兼容已含新列的新库。
  - 注释导出：命令面板"导出注释..."，`QSaveFile` 原子写 JSON；只导出有标签/收藏/metadata 的行，读取用 3 次批量查询（`getAnnotatedAssets` / `getAllMetadata` / `getAllAssetTags`），不做 N+1。
  - 缺失可见：状态栏显示"N 张图片 · M 个文件缺失"；命令面板"清理缺失记录..."二次确认后批量删除，并关闭详情面板中已删除的条目。
  - 基准：新增 `pixobook_bench`（合成库，可配 `--assets/--runs`）。10k：导入 0.65s、常用词搜索 p50 31ms、深页(offset 2000) 53ms；50k：常用词 p50 308ms、深页 501ms —— 证明 `LIKE '%kw%'` 全表扫描是核心瓶颈，FTS5 是下一轮。
  - 工程：CMake 的 `GLOB_RECURSE` 加 `CONFIGURE_DEPENDS`；修复两处 `[[nodiscard]]` 返回值丢弃；本地 Qt 5.13.2 x64 与 CI 的 5.15.2 Win32 不一致的问题已记录（尚未统一）。
  - 测试：`tst_database` 新增缺失状态保留/更新清除/只删缺失/导出批量查询/v2→v3 迁移+备份；`tst_librarycontroller` 新增"删除不丢标签且可恢复"、"重扫恢复回归文件"、"导出 JSON 内容"。
  - 构建 0 错 0 警告，38/38 扫描器 PASS，6/6 测试通过。

- **第 12 轮（工作区）— FTS5 检索 + 分页查询去物化**：
  - 新增 FTS5 虚表 `asset_search(asset_id UNINDEXED, file_name, prompt, negative_prompt)`，`rowid` 与 `assets.rowid` 对齐；`ensureSearchIndex()` 启动时创建、清理孤儿行并按 `asset_id` 补建缺失索引；`insertAsset`/`updateAsset`/`upsertMetadata`/`deleteAsset`/`deleteMissingAssets` 全部同步。FTS5 不可用时自动回退 LIKE。
  - 混合检索：先查 `COUNT(*) FROM asset_search WHERE MATCH`，命中 < 全库 5% 时走 FTS 物化+排序；命中密集（如 "portrait" 占 55%）时走 `created_at` 索引序的 LIKE 扫描，命中一页即停。总数为单独的 `countAssets()` 查询，移除 `DISTINCT` + `COUNT(*) OVER()`；标签过滤改 `EXISTS`（不再 JOIN 出重复行）。
  - CJK：索引时在每个汉字/假名/谚文两侧加空格（unicode61 逐字成 token），查询把 CJK 连续段构造成 `"月 光"` 短语；ASCII 段用前缀 `word*`。Qt 5.13.2 自带 SQLite 无 trigram tokenizer，这是无升级依赖的替代方案。
  - 结果（本机）：10k 常用词 3.0ms（原 31ms）、深页 4.2ms（原 53ms）、CJK 1.0ms；50k 常用词 11.2ms（原 308ms）、深页 9.8ms（原 501ms）。
  - 导入性能：扫描期间 `setDeferSearchIndexing(true)` 跳过逐行 reindex，`scanFinished` 在提交后调用 `indexPendingAssets()` 一次性建索引（单条 prepared INSERT 循环）。10k 0.73s、50k 5.4s，基本回到无索引基线（0.65s/5.2s）。延迟只对本次扫描新插入的 id 生效（`m_deferredNewAssets`）；扫描期间编辑已有素材的 prompt 仍立即更新索引。
  - 修复：`readAssetRow()` 用 `QSqlRecord::indexOf` 判断别名列，消除 `unknown field name` 警告；迁移备份前 `q.finish()`，避免 `wal_checkpoint(FULL)` 被活动语句锁住。
  - 测试新增 5 项：FTS 前缀/元数据更新/改名/删除同步、CJK 子串、清理缺失同时清索引、删表后自动重建、延迟索引只跳过新行。
  - 构建 0 错 0 警告，39/39 扫描器 PASS，6/6 测试通过（tst_database 21 项）。

- **第 13 轮（工作区）— 缺失核对 + 注释回灌**：
  - 缺失核对：`LibraryController::reconcileMissingFiles()` 在后台线程对**正在监听的文件夹**内的在线条目做 `QFileInfo::exists`，缺失者在 GUI 线程批量 `markAssetMissing`；扫描结束 1s 去抖后自动触发，也可手动调用。只核对监听目录，避免把"已停止监听、盘未挂载"的素材误标缺失。新增 `missingCheckFinished(int)` 信号，MainWindow 弹 Toast/更新状态栏。
  - 注释回灌：`importAnnotations()` 读取导出的 JSON（format/version 校验，64MB 上限），按 hash 优先、path 兜底匹配；收藏只置真、标签按名字并集（缺标签自动创建）、metadata 只在本地为空时填充。单事务提交，返回 matched/unmatched/tagsCreated/favoritesSet/metadataFilled 统计；命令面板"导入注释..."展示合并摘要。
  - 测试新增 2 项：关闭期间删除的文件被核对标记缺失且标签保留；注释 JSON 按 hash 合并、未知素材计 unmatched、标签/收藏/metadata 生效。
  - 构建 0 错 0 警告，39/39 扫描器 PASS，6/6 测试通过（tst_librarycontroller 16 项）。

- **第 14 轮（工作区）— 相关度排序 + 缺失重定位 + iTXt**：
  - 相关度排序：`searchAssets(..., sortField="relevance")` 强制走 FTS 路径，`ORDER BY bm25(asset_search) ASC, a.id ASC`；无关键词或 FTS5 不可用时回退 created_at。搜索栏新增"相关度"选项。基准：选择性词 1.5ms（10k）/12.8ms（50k）。
  - 缺失重定位：新增"查看缺失文件..."对话框（列表 + 重新定位 + 移除记录）。`LibraryController::relinkAsset()` 用 `scanSingleFile` 读取新文件，校验目标路径未被其他记录占用，复用原 id 更新路径/尺寸/格式/hash/文件夹并清除 `missing_since`；标签与收藏保留，本地 metadata 为空时解析新文件。新路径不会自动加入监听列表（避免隐式扩大监听范围）。
  - iTXt：SDParser 支持 PNG iTXt（keyword/压缩标志/语言/翻译关键字/UTF-8 文本），含 zlib 压缩（flag=1, method=0）；`containsSdMetadata` 与 tEXt/zTXt 共用关键字探测。
  - 测试新增 3 项：BM25 短文档优先、缺失记录重定位保留标签、iTXt 未压缩（中文）+ 压缩解析。构建 0 错 0 警告，39/39 扫描器 PASS，6/6 测试通过（tst_database 22、tst_librarycontroller 17、tst_parsers 10）。

- **第 15 轮（工作区）— 缺失恢复的视觉设计**：
  - `MissingFilesDialog` 重做：无边框圆角深色面板、可拖动标题区；琥珀色（需关注）与青绿色（健康）状态语言；标题/副标题/数量徽章/关闭按钮；筛选行；自绘行（52px 缩略图占位、琥珀状态点、文件名/路径、相对时间、收藏星标、标签数 chip）；空状态（绿色对勾 / 搜索图标）；底部提示 + 主/危险/次按钮。
  - 交互：从资源管理器拖文件到条目直接重定位；Enter 重定位、Delete 移除、Ctrl+F 聚焦筛选、Esc 关闭；内联通知条替代弹窗错误；重新定位后列表即时刷新；窗口高度按缺失数量 400–560px 自适应。
  - 设计系统：`CustomStyle` 增加 `primary`/`danger` 按钮变体，并修复禁用态仍保留饱和填充的问题；新增 `ACCENT_HOVER/PRESSED`、`WARNING/WARNING_BG/WARNING_BORDER`、`SUCCESS`；修正 Codicon codepoint（`image`/`file-media` = 0xeaea，`warning` = 0xea6c）。
  - 状态栏缺失计数改为可点击链接，直接打开恢复面板。
  - 测试：`tst_ui` 用临时库渲染“有缺失/无缺失”两张参考图到 `build/shots`；测试目标编译 `resources.qrc` 以加载 Codicon。
  - 构建 0 错 0 警告，39/39 扫描器 PASS，6/6 测试通过。

#### In Progress
- 无

#### Blocked
- 无（tst_filestorage 已在第 6 轮解决：原"静默崩溃"实为 `testScanSingleFile` 的路径分隔符断言错误——`QFileInfo::absoluteFilePath()` 返回正斜杠，测试却用 `QDir::toNativeSeparators()` 比较，已改为 `fromNativeSeparators()`；另新增 computeDiff 3 项 + per-dir 隔离集成测试，4/4 全绿）

### Key Decisions
- 双 SQL 查询（countAssets + searchAssets）用 `COUNT(*) OVER()` 窗口函数合并为单次（第 12 轮已撤销：该窗口函数迫使 SQLite 评估全部命中，50k 深页 501ms；现改为独立 count 查询，CHECK 39 拦截回归）
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
- 文件夹筛选按 `file_path` 前缀匹配（`= prefix OR LIKE prefix/% ESCAPE '\'`），因为 `Asset.folderId` 是文件所在目录，递归扫描后子目录文件不属于根目录；`%`/`_` 必须转义
- 数据变更后的刷新用 `GalleryWidget::refreshAssets()`：按 asset id 恢复选择、保留 scroll，并按 `max(200, 已加载数)` 重新查询，避免收藏/标签操作把列表跳回顶部或截断已加载页
- 文件夹"移除"定义为停止监听而非删除数据，菜单文案与确认框显式说明素材保留；真正删数据只走素材删除流程
- "最近导入"tab 通过 `SearchBar::setSortSelection()` 同步排序控件，查询层不再静默覆盖用户排序
- 重扫时 `FileScanner` 接收已知 path 集合，在尺寸/哈希/元数据解析之前跳过已导入文件；scanStarted 的全量 path/hash 快照仍负责跨路径的内容去重
- `tst_database` 每个用例使用独立数据库文件，保证 `tst_database <single-test>` 也能通过
- 文件重命名/移动的识别条件：同 hash、不同 path、旧 path 已不存在；此时 `updateAsset` 复用原 id 行（标签/元数据保留），不新建也不删除
- FileWatcher 卸载按路径边界（`== root || startsWith(root + '/')`）判断，避免 `C:/Photos` 误伤 `C:/Photos2`
- Sidebar 的 `setActiveFolder()` 会清除标签高亮，`setActiveTag()` 会清除文件夹高亮；选择标签时必须用后者
- 数据分两层：路径/尺寸/hash/prompt 等是**派生数据**（重扫可重建），标签/收藏/备注是**原生数据**（只能显式删除）。`assets.missing_since` 表达"文件暂时不可达"，把"文件不在"与"用户要删"分开；清空标记只发生在文件重新可达时
- `fileRemoved` 是状态事件而非删除意图：标记缺失 + `emit dataChanged`，绝不 `deleteAsset`；批量清理只走 `deleteMissingAssets()`（用户二次确认）
- 恢复扫描必须让缺失路径重新进入 scanner：`scanFolder` 的 knownPaths 只放在线条目，`assetFound` 先查 `m_scanMissingAssets` 再查 known 集合，否则同 path 会被"已导入"跳过而永远无法恢复
- 注释导出只包含有标签/收藏/metadata 的行，用 3 次批量查询 + `QSaveFile` 原子写；JSON 保留 `hash`/`filePath` 作为将来导入时的匹配键
- 迁移前先 `wal_checkpoint(FULL)` 再复制 `*.bak-v{n}`；v3 的 `ALTER TABLE` 用 `PRAGMA table_info` 判重，避免新库（createSchema 已含列）重复加列失败
- 性能工作以 `pixobook_bench` 的数字为准：没有 before/after 就不做"优化"
- FTS5 索引是**派生数据**：可随时 `DROP TABLE asset_search`，下次启动自动重建；`asset_search.rowid` 必须等于 `assets.rowid`（reindex 按 rowid O(1) 删旧行），因此**不能对该库执行 VACUUM**（会重写 rowid）。
- 检索策略按命中密度分流：FTS 会物化并排序全部命中，适合选择性词；密集词用 `created_at` 索引序 LIKE 扫描 + LIMIT 提前结束更快。阈值取 `matches * 20 <= totalAssets`（<5%）。
- 总数不要用 `COUNT(*) OVER()`：它让 SQLite 在 LIMIT 之前评估所有命中，10k 深页从 5ms 退化到 53ms。查询总数单独执行；FTS 计数不需要 join。
- 标签过滤用 `EXISTS` 而不是 `LEFT JOIN asset_tags`：JOIN 会产生重复行，逼出 `DISTINCT`，而 `DISTINCT` 又会阻止 LIMIT 提前结束。
- CJK 用"逐字空格 + 短语查询"而不是 trigram：Qt 5.13.2 的 SQLite 3.28 没有 trigram tokenizer；升级 SQLite 前这是零依赖方案。ASCII 搜索是词前缀语义（"port" 命中 "portrait"），不再是任意中间子串。
- 扫描导入时延迟建索引：`setDeferSearchIndexing(true)` → 批量写 assets/metadata → commit → `indexPendingAssets()`（一条 prepared INSERT 循环）。逐行 reindex 会让 50k 导入从 5.4s 退化到 21.5s。
- 延迟只对本次扫描新插入的 id 生效（`m_deferredNewAssets`）：扫描期间用户编辑已有素材的 prompt/文件名仍走立即 reindex，避免索引在扫描结束前过期。
- `indexPendingAssets()` 同时是启动补建入口：查询 `LEFT JOIN asset_search` 找缺失行并批量插入；它不删除已有行，删除/改名仍走 `reindexAsset()` 的 rowid 删除+插入。
- 缺失核对只覆盖 `m_folders` 内的路径：停止监听的文件夹可能位于暂时未挂载的磁盘，绝不能因为"没在监听"就标记缺失；这与 `fileRemoved` 的语义一致（不可达 ≠ 删除）。
- 核对线程只做 `QFileInfo::exists`；所有 DB 写入通过 `QMetaObject::invokeMethod` 排队回 GUI 线程（QSqlDatabase 连接有线程亲和性）。`QPointer` guard 防止控制器销毁后回调。
- 扫描结束后的核对延迟 1000ms 去抖，让 watcher 先送达删除事件；`m_reconcileRunning` 防止重叠，重复调用直接返回。
- 注释导入是**合并**而不是覆盖：收藏只置真、标签并集、metadata 仅在本地 `source` 为空时写入。没有文件字节时无法新建素材，匹配不到的条目计入 `unmatched` 并跳过。
- 导入按 hash 优先匹配（内容身份），path 兜底（文件移动但未重扫）；同一 hash 多行时 `findByHash` 返回第一行，属于已知限制。
- 相关度排序必须强制走 FTS：`bm25()` 只能引用 FROM 中的 `asset_search`，而密集词默认走 LIKE。排序为 `bm25 ASC, a.id ASC`（bm25 越小越相关），无关键词时回退 created_at。
- 重定位复用原 id：`scanSingleFile` 重新计算物理字段，`findByPath` 排除自身后判重，再 `updateAsset` 清 `missing_since`。标签/收藏天然保留；本地 metadata 为空时才解析新文件。
- 重定位的新路径不会自动加入监听列表：隐式扩大监听范围会让用户意外暴露新的目录；需要监听应由用户显式添加。
- iTXt 的 keyword 与 tEXt/zTXt 相同，因此 `containsSdMetadata` 只需识别 chunk 类型；`tryParseFromData` 额外跳过压缩标志/语言/翻译关键字，flag=1 且 method=0 时 `qUncompress`。
- 缺失恢复的视觉语言：琥珀 = 可恢复的关注（不是错误），青绿 = 一切在线，蓝色 = 主操作；危险操作只用红色描边，不使用红色实底。
- 恢复面板的行由 delegate 自绘：缩略图占位 + 状态点 + 相对时间 + 收藏/标签 chip；相对时间在插入条目时预计算（RoleAgeText），paint 内不做字符串分配。
- 拖放是首选重定位手势：文件直接落到某一行；按钮打开文件选择器作为可达性兜底；错误用内联通知条，不打断列表。
- 禁用态的主/危险按钮必须退回中性底色，否则用户会以为操作仍可用（CustomStyle 统一处理）。
- Codicon codepoint 必须对照 `resources/codicon.css`；`image`/`file-media` 是 0xeaea，0xeada 是摄像头。
- 无边框半透明 QDialog 在 Windows 上无需阴影也可读：1px BORDER + 10px 圆角即可；拖动只绑定标题区，避免与列表拖放冲突。
- 测试渲染截图需 `QT_QPA_PLATFORM=windows`（offscreen 无字体），且测试目标必须编译 `resources.qrc` 才能加载 Codicon。

### Next Steps
- 批量重定位：选择新文件夹后按 hash 自动匹配所有缺失记录（当前一次只能重定位一条）；同 hash 命中时复用 `relinkAsset` 的单条逻辑。
- 注释导入的冲突预览：当前静默合并，后续可在导入前展示 matched/unmatched/将新建标签列表供确认。
- 备份保留策略：`*.bak-v{n}` 目前只增不删，后续只保留最近 N 个。
- 把 `pixobook_bench` 纳入 CI 趋势记录（至少每轮记录 10k/50k 的 p50/p95 数字）。
- 新增任何后台解码路径时，worker 只返回 `QImage`，`QPixmap` 必须在 GUI 线程构造（CHECK 37 会拦截）
- 扫描期新增/删除文件的路径必须同步更新 `m_scanKnownPaths` / `m_scanKnownHashes`，否则去重快照会漂移
- 可继续优化扫描的 metadata 写入：目前仍是每文件一次 prepared upsert，后续可批量/复用 QSqlQuery
- `scanFolder` 仍用 `getAllAssets()` 建 known 集合，可加只查 `file_path` 的轻量接口，减少重扫前的整行读取
- AddTag undo 构造仍是每 asset 一次查询，后续加批量"已有该标签的 asset 集合"接口
- UI 测试目前只覆盖 Gallery 的 refresh/selection，Lightbox 点击区域、DetailPanel 编辑、MainWindow 文件夹筛选仍缺 widget 级测试
- 若再发现局部 update(QRect) 但 paintEvent 全量重绘的自绘控件，复用第 7 轮脏区域窄化模式（如 LightboxWidget/SidebarWidget 的热区变化）

### Critical Context
- `QFontMetrics` 构造在 Qt5 中涉及字体引擎初始化，在 paintEvent 循环中被多次调用时产生可测量的每帧分配
- 异步图片加载需要 `#include <QtConcurrent>` + `#include <QPointer>` 在 .cpp 中，`m_loadGeneration` 在 .h 中声明为 `int`，以 0 初始化
- `QImageReader::read()` 对高分辨率图片（>4K）可阻塞主线程 200-500ms，离线解码后 `invokeMethod` 回主线程设置 QPixmap + update()
- `QPixmap` 绑定 GUI 线程，工作线程只能解码 `QImage`；跨线程返回 `QPixmap` 在部分平台/驱动上会崩溃或渲染异常
- ImageCache `finishThumbnail` 必须在同一把 `m_mutex` 下先 erase `m_pending` 再 `insertLocked`，emit 放到锁外
- 扫描快照期间 FileWatcher 的 fileAdded/fileRemoved 必须同步维护集合；并发扫描通过 `m_scanDepth` / `m_scanTxDepth` 配对提交
- `DELETE`/写操作失败后 `rollbackTransaction()` 也可能失败，调用方应以操作返回值为准，不能假定回滚成功
- `assets.missing_since` 为空表示文件在线；`Asset::isMissing()` 是唯一判断入口。新增任何 `SELECT` 映射都要走 `readAssetRow()`，否则会漏掉该列
- 迁移版本 3 的备份文件是 `<db>.bak-v2`（源版本号），只在 `version > 0 && version < 3` 时创建；新库不会产生备份
- `FileWatcher::fileRemoved` → `markAssetMissing`；恢复由 `fileAdded`、手动导入或 `scanFolder` 触发。不要重新引入"文件消失即删行"
- 扫描恢复时同时维护 `m_scanMissingAssets` / `m_scanKnownPaths` / `m_scanKnownHashes`，三者不一致会导致重复行或恢复失败
- `pixobook_bench` 是控制台程序，运行时需要 Qt bin 在 PATH（`D:\Qt5.13.2\5.13.2\msvc2017_64\bin`），否则缺 DLL 静默退出
- 本机 `build/` 的 CMake 缓存必须指向 64 位 Qt（msvc2017_64）；`Qt5_DIR` 若解析到 msvc2017（x86）会在链接期报 677 个未解析符号
- Qt 5.13.2 自带 SQLite 已编入 FTS5（bench 输出 `search backend FTS5` 可确认）；但没有 trigram tokenizer，CJK 依赖 `normalizeForFts()` 的逐字空格方案
- `asset_search` 是普通 FTS5 表（不是 external content）：`rowid = assets.rowid`，`asset_id` 仅作 join/unindexed 列。reindex 必须按 rowid 删除旧行，否则会留下重复索引项
- `ensureSearchIndex()` 每次 initialize 都执行：建表 → 清理孤儿 → 补建缺失行。它容忍 FTS5 缺失（`m_ftsAvailable=false`，搜索退回 LIKE），迁移失败不应阻止应用启动
- 检索阈值 `matches * 20 <= totalAssets`（<5%）是经验值：低于阈值走 FTS，高于阈值走索引序 LIKE。修改阈值必须用 `pixobook_bench` 的 10k/50k 数字验证
- `searchAssets` 的总数由 `countAssets()` 单独查询；两者必须使用完全相同的过滤条件，否则分页总数会漂移
- FTS 同步期间不要对该库执行 `VACUUM`：VACUUM 会重写 rowid，破坏 `asset_search.rowid = assets.rowid` 的约定；若必须 VACUUM，先 `DROP TABLE asset_search` 再重开应用重建
- 缺失核对自动触发的时机是 `scanFinished` 后 1000ms；若用户在这 1s 内又启动扫描，定时器会重置（单次），最终只核对一次
- 注释导入上限 64MB；JSON 顶层必须是 `format: "pixobook-annotations"`。导入失败会回滚整个事务，不会留下半合并状态
- 相关度排序需要关键词 + FTS5；无关键词时即使选"相关度"也按 created_at 返回，不会报错
- 重定位不改变素材 id，因此标签/收藏/撤销栈都仍指向同一行；新路径若已被另一条记录占用会被拒绝
- iTXt 只支持压缩方法 0（zlib）；其他方法跳过该 chunk，不会误读文本

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
- `src/core/IDatabaseManager.h` / `src/database/DatabaseManager.*`：`searchAssets`/`countAssets` 增加 `folderPath`，前缀 LIKE + 转义
- `src/ui/MainWindow.h/.cpp`：`m_activeFolderPath`、`refreshAssets()`/`applyAssets(bool)`、文件夹筛选/停止监听确认、导入结果提示、删除详情判断
- `src/ui/GalleryWidget.*`：`refreshAssets()` 按 id 恢复选择并保留滚动
- `src/ui/SearchBar.*`：`setSortSelection()` 同步排序下拉（不触发 filterChanged）
- `src/services/FileScanner.*`：`scanDirectory(..., knownPaths)` 在昂贵 I/O 前跳过已导入文件；birthTime 回退
- `src/services/FileWatcher.cpp`：新子目录加入待扫描队列
- `src/parsers/SDParser.cpp`：PNG 签名前置检查
- `src/ui/LightboxWidget.cpp`：顶/底 overlay 栏点击不再关闭
- `tests/tst_database.cpp`：每用例独立数据库 + 文件夹前缀筛选测试
- `tests/tst_librarycontroller.cpp`：known path 跳过、新子目录监听、commit 失败回滚、导入返回值
- `tests/tst_parsers.cpp`：真实 PNG tEXt/zTXt 解析
- `tests/tst_ui.cpp`：GalleryWidget refresh 保持选择
- `ci/scan-bug-patterns.ps1`：39 个检查块，自动扫描（新增 CHECK 38/39）
- `src/models/Asset.h`：新增 `QDateTime missingSince` + `isMissing()`
- `src/core/IDatabaseManager.h`：新增 missing 状态 API（`markAssetMissing`/`markAssetPresent`/`countMissingAssets`/`getMissingAssets`/`deleteMissingAssets`）与导出批量查询（`getAnnotatedAssets`/`getAllMetadata`/`getAllAssetTags`）
- `src/database/DatabaseManager.h/.cpp`：schema v3（`missing_since` + 部分索引）、`readAssetRow()` 统一行映射、迁移前 checkpoint+备份、`hasAssetColumn()` 判重
- `src/services/LibraryController.h/.cpp`：watcher 删除语义改为标记缺失、`restoreMissingAsset()`、`m_scanMissingAssets`、`exportAnnotations()`（QSaveFile JSON）、`cleanupMissingAssets()`
- `src/ui/MainWindow.h/.cpp`：状态栏缺失计数、命令面板"导出注释/清理缺失记录"、导出后状态提示
- `bench/bench_main.cpp` + `CMakeLists.txt`：`pixobook_bench` 目标、`CONFIGURE_DEPENDS`
- `tests/tst_database.cpp`：缺失状态/更新清除/只删缺失/导出查询/v2→v3 迁移备份
- `tests/tst_librarycontroller.cpp`：删除不丢标签并可恢复、重扫恢复、导出 JSON
- `src/database/DatabaseManager.*`：FTS5 `asset_search` 虚表、`ensureSearchIndex()`/`reindexAsset()`、`normalizeForFts()`/`buildFtsMatchQuery()`、混合检索与独立 count、`isFullTextSearchEnabled()`
- `tests/tst_database.cpp`：FTS 前缀/更新/改名/删除同步、CJK 子串、清理缺失同步清索引、删表自动重建
- `ci/scan-bug-patterns.ps1`：CHECK 39 `window-count-materialization`
- `bench/bench_main.cpp`：新增 CJK 查询用例与 search backend 输出
- `src/services/LibraryController.h/.cpp`：`reconcileMissingFiles()`（后台 stat + 1s 去抖）、`missingCheckFinished` 信号、`importAnnotations()`（hash/path 匹配、合并策略、统计）
- `src/ui/MainWindow.h/.cpp`：缺失核对 Toast/状态、命令面板"导入注释..."与合并摘要
- `tests/tst_librarycontroller.cpp`：关闭期间删除的核对测试、注释导入合并测试、缺失重定位（17 项）
- `src/database/DatabaseManager.cpp`：相关度排序强制 FTS + `bm25(asset_search)`（含 `id` 稳定次序）
- `src/ui/MissingFilesDialog.h/.cpp`：缺失文件列表、重新定位、移除记录
- `src/services/LibraryController.h/.cpp`：`relinkAsset()`（scanSingleFile + 路径冲突检查 + 清缺失 + 空 metadata 补解析）
- `src/parsers/SDParser.cpp`：iTXt（UTF-8 / zlib）解析；`containsSdMetadata` 识别 iTXt
- `src/ui/SearchBar.cpp`：排序下拉新增"相关度"
- `tests/tst_database.cpp`：BM25 短文档优先（22 项）
- `tests/tst_parsers.cpp`：iTXt 未压缩中文 + 压缩（10 项）
- `bench/bench_main.cpp`：新增 relevance 查询用例
- `src/ui/MissingFilesDialog.h/.cpp`：恢复面板重设计（自绘 delegate、拖放、筛选、空状态、内联通知）
- `src/ui/CustomStyle.cpp`：`primary`/`danger` 按钮变体 + 禁用态中性化
- `src/ui/ColorConstants.h`：`ACCENT_HOVER/PRESSED`、`WARNING*`、`SUCCESS`
- `src/ui/Codicon.h`：`image`/`file-media`/`warning` 映射修正
- `src/ui/MainWindow.cpp`：状态栏缺失计数改为可点击链接
- `tests/tst_ui.cpp`：临时库渲染两张恢复面板参考图到 `build/shots`
- `CMakeLists.txt`：测试目标编译 `resources.qrc`
- `docs/images/missing-files-dark.png` / `missing-files-empty.png`：设计参考图

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
| 39 | 数据变更后重新 `setAssets()` | 收藏/加标签后滚动位置与选择丢失，列表跳回顶部 | 刷新路径用 `refreshAssets()` 按 id 恢复选择并保留 scroll |
| 40 | 批量删除后只检查第一项是否在详情面板 | 详情面板可能继续显示已删除素材 | 遍历所有被删 id 判断 `currentAssetId()` |
| 41 | FileSystemWatcher 只 `addPath` 新子目录、不补扫 | watcher 注册前已存在的文件永远不会导入 | 新子目录同时加入待扫描队列，触发 `checkForNewFiles()` |
| 42 | 滚轮/拖动后把 scrollOffset 吸附到整行 | 单格滚轮（15px）被四舍五入回原行，滚动看起来完全无效 | 滚动位置保持像素精度；仅键盘导航按 item 对齐 |
| 43 | 重命名/移动后先 added 后 removed，added 因同 hash 被跳过 | 随后 removed 删除旧行：素材、标签、元数据全部丢失 | 同 hash + 旧 path 不存在时 `adoptRenamedAsset()` 复用原行更新路径 |
| 44 | 路径前缀 `startsWith(root)` 无分隔符边界 | 移除 `C:/Photos` 会误停 `C:/Photos2` 的监听 | 使用 `== root || startsWith(root + '/')` |
| 45 | 互斥 setter 误用（`setActiveFolder("")` 清标签高亮） | 选中的标签在侧栏立即失去高亮 | 选标签用 `setActiveTag()`，其内部会清文件夹高亮 |
| 46 | 监听器把"文件消失"直接 `deleteAsset` | 外置盘/网络盘/同步目录一次抖动就永久丢失标签与收藏（唯一不可恢复的失败） | 检查 `fileRemoved` 回调是否只标记 `missing_since`；删除必须由用户显式触发（CHECK 38 会拦截） |
| 47 | 分页总数用 `DISTINCT` + `COUNT(*) OVER()` | 迫使 SQLite 物化并统计全部命中，深页随库线性变慢（50k 501ms），LIMIT 无法提前结束 | 总数单独查询；标签过滤用 `EXISTS` 消除重复行（CHECK 39 会拦截） |
| 48 | 只依赖 watcher 发现文件删除 | 应用关闭期间的删除永远不标记缺失，状态栏与筛选长期失真 | 扫描结束后对监听目录做一次存在性核对（后台 stat，只覆盖 watched folders） |
| 49 | 重定位只改路径、未校验目标已被占用 | 两条记录指向同一文件，派生字段与标签归属错乱 | 先 `scanSingleFile` 取绝对路径，再用 `findByPath` 排除自身后判重 |
| 50 | 禁用态主/危险按钮仍保留饱和填充 | 用户以为操作可用，点击无响应；空状态尤其误导 | 在 `CE_PushButton` 中对 `!enabled` 覆盖 bg/border 为中性色 |

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
| 文件夹点击不筛选 | IDatabaseManager.h, DatabaseManager.*, LibraryController.*, MainWindow.* | `folderPath` 前缀匹配（根 + 子目录），`m_activeFolderPath` 接入首屏/分页 |
| 停止监听文案与行为不符 | SidebarWidget.cpp, MainWindow.cpp | 菜单改"停止监听此文件夹"，确认框说明素材保留 |
| 刷新跳顶/丢选择 (模式 39) | GalleryWidget.*, MainWindow.cpp | `refreshAssets()` 按 id 恢复选择、保留 scroll、保留已加载页数 |
| 批量删除详情残留 (模式 40) | MainWindow.cpp | 遍历全部被删 id 而非只比较第一项 |
| 最近导入排序控件无效 | MainWindow.cpp, SearchBar.* | tab 切换同步排序下拉，移除查询层静默覆盖 |
| Lightbox 顶/底栏误关闭 | LightboxWidget.cpp | overlay 栏点击直接忽略，不进入"点击外部关闭" |
| 重扫重复哈希/解析 | FileScanner.*, LibraryController.cpp | knownPaths 在尺寸/哈希/元数据前跳过；birthTime 无效回退 |
| 非 PNG 整文件 readAll | SDParser.cpp | 先读 8 字节签名，失败立即返回 |
| 新子目录文件漏导入 (模式 41) | FileWatcher.cpp | 新子目录加入 pending 队列补扫 |
| 导入成功提示不准确 | LibraryController.*, MainWindow.cpp | `scanAndInsertFile` 返回 bool，Toast 区分成功/重复 |
| 测试用例相互依赖 | tests/tst_database.cpp | 每个用例独立数据库文件，支持单测运行 |
| 滚动条滚轮失效 (模式 42) | GalleryWidget.cpp | 移除 scrollToPos/wheelEvent 的行吸附；滚轮步长 angleDelta/2；轨道点击按 thumb 居中；thumb 命中区覆盖整条轨道 |
| 详情面板滚轮过慢 | DetailPanel.cpp | angleDelta/8 → /2，与画廊一致 |
| 重命名丢失素材/标签 (模式 43) | LibraryController.h/.cpp | `adoptRenamedAsset()`：同 hash、旧 path 不存在时更新原 asset 的路径并保留 id/标签/元数据 |
| 卸载误伤兄弟目录 (模式 44) | FileWatcher.cpp | `pathIsSameOrUnder()` 按路径分隔符边界判断，覆盖 Windows/Unix 根路径 |
| 标签选中后无高亮 (模式 45) | MainWindow.cpp | tagSelected 改用 `setActiveTag()`，不再调用会清标签的 `setActiveFolder("")` |
| 拖拽文件夹侧栏不更新 | MainWindow.cpp | gallery filesDropped 后 `setFolders(m_library->folders())` |
| 文件消失即删数据 (模式 46) | DatabaseManager.*, LibraryController.*, IDatabaseManager.h, Asset.h | schema v3 新增 `missing_since`；watcher 只标记缺失；恢复走 fileAdded/手动导入/重扫；清理缺失需用户二次确认 |
| 迁移无备份/新库重复加列 | DatabaseManager.cpp | 升级前 `wal_checkpoint(FULL)` + `*.bak-v{n}`；`ALTER TABLE` 前用 `PRAGMA table_info` 判重 |
| 缺失文件无法恢复 | LibraryController.cpp | `scanFolder` 的 knownPaths 排除缺失；`assetFound` 先查 `m_scanMissingAssets` 再查 known，`restoreMissingAsset()` 保留 id/标签/收藏 |
| 分页总数窗口物化 (模式 47) | DatabaseManager.cpp | 移除 `DISTINCT` + `COUNT(*) OVER()`，改独立 `countAssets()`；标签过滤 `EXISTS`；50k 深页 501ms→8ms |
| 全文检索全表扫描 | DatabaseManager.h/.cpp | FTS5 `asset_search` + 混合检索（选择性词走 FTS，密集词走索引序 LIKE）+ CJK 逐字短语 |
| 关闭期间删除的文件未标缺失 (模式 48) | LibraryController.h/.cpp | `reconcileMissingFiles()` 后台 stat 监听目录，scanFinished 后 1s 去抖触发，GUI 线程批量标记；只覆盖 watched folders |
| 注释无法回灌 | LibraryController.h/.cpp, MainWindow.h/.cpp | `importAnnotations()` 按 hash→path 匹配，标签并集/收藏置真/metadata 仅在空时填充，单事务 + 统计摘要 |
| 缺失记录无法重定位 (模式 49) | LibraryController.h/.cpp, MissingFilesDialog.* | 复用原 id 更新物理字段并清缺失；路径冲突拒绝；标签/收藏保留 |
| 搜索只有时间序 | DatabaseManager.cpp, SearchBar.cpp | 新增"相关度"排序，强制 FTS + `bm25`，1.5ms/12.8ms（10k/50k） |
| iTXt 元数据不可见 | SDParser.cpp | 支持 UTF-8 与 zlib 压缩的 iTXt，含中文 prompt |
| 禁用按钮看似可用 (模式 50) | CustomStyle.cpp | `!enabled` 时主/危险按钮统一退回 `BG_BUTTON_SUBTLE` + `BORDER_MUTED` |
| 图标字形错误 | Codicon.h | `image`/`file-media` 修正为 0xeaea，新增 `warning` 0xea6c / `error` 0xea87 |
| 恢复面板视觉与交互 | MissingFilesDialog.*, CustomStyle.cpp, ColorConstants.h | 无边框深色面板、自绘行、拖放重定位、筛选、空状态、内联通知；主/危险按钮变体 |

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
| CHECK 38（新增） | 检测 `fileRemoved` 回调 20 行内出现 `deleteAsset(` — 文件消失必须标记缺失而非删除；剥离 `//` 注释，遇 `});` 结束窗口，避免把后续函数误判 |
| CHECK 39（新增） | 检测 `COUNT(*) OVER()` — 分页总数窗口函数会迫使 SQLite 物化全部命中，应改为独立 count 查询（FTS 计数无需 join） |

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
