# PixoBook — 递归自我改进

本文件指导 opencode agent 在代码变更后自动进行静态分析和质量检查。

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
| 12 | Windows 路径比较未忽略大小写 | 路径匹配失败 | 检查 `.startsWith(path)` / `==` 是否使用 `Qt::CaseInsensitive` |
| 13 | 事务没有回滚路径 | 部分执行后数据库不一致 | 检查 `beginTransaction` 后，分支路径是否都有 `rollback` |

## 已修复的实例

| Bug 模式 | 修复文件 | 修复内容 |
|----------|---------|----------|
| QtConcurrent UAF (模式 5) | `FileScanner.cpp`, `ImageCache.cpp` | `[this]` → `QPointer` guard |
| 路径大小写 (模式 12) | `FileWatcher.cpp` | `startsWith` 添加 `Qt::CaseInsensitive` |
| 死 slot (模式 4) | `FileWatcher.h` | 移除未使用的 `fileModified(QString)` 信号 |
| paintEvent 堆分配 (模式 10) | GalleryWidget, DetailPanel, TabBar, TitleBar, SidebarWidget, LightboxWidget, SearchBar | QPainterPath → drawRoundedRect; QFont → 成员变量缓存; QString::arg → 预计算 |
| 跨线程信号未注册 (模式 17) | Asset.h, main.cpp | 添加 Q_DECLARE_METATYPE(Asset) + qRegisterMetaType<Asset>() |
| QPainterPath 未使用 include | DetailPanel, TitleBar, LightboxWidget, TabBar, SearchBar | 移除多余的 `#include <QPainterPath>` |
| 辅助绘制方法未标记 `const` (模式 14) | GalleryWidget, DetailPanel | `drawEmptyState`, `drawRubberBand`, `drawSectionDivider`, `drawSummaryAction`, `drawField`, `drawScrollIndicator` → 全部加 `const` |

## 扫描器改进

| 改进 | 说明 |
|------|------|
| CHECK 1 增强 | QObject 类 + `= default` 析构函数跳过检查（依赖注入模式） |
| CHECK 1 增强 | 检查成员类型在 .cpp 中是否被 `new` 分配，仅标记实际拥有的裸指针 |
| CHECK 5 精确化 | 只检查 `QtConcurrent::run([this])` 中的 `[this]` 捕获，排除 `QPointer guard(this)` 上下文 |
| CHECK 14 精确化 | 通过函数名回溯判断 `.arg()` 是否真正在 paintEvent 函数体内，消除跨函数误报 |
| CHECK 14 增强（模式 14） | 辅助绘制方法未标记 `const` — 新增检查项并添加 scanner 规则 |
| CHECK 17（新增） | 检测信号参数中缺少 Q_DECLARE_METATYPE 的自定义类型 — 自动扫描所有 Q_OBJECT 类的信号签名 |

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
