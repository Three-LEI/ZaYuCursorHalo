# Windows 7 兼容性硬约束

> 生效日期：2026-11-20  
> 适用范围：ZaYuCursorHalo 的所有新功能、重构、依赖升级和发布构建。  
> 原则：**Windows 7 SP1 x86 是第一目标；兼容性不是发布前补丁，而是功能准入条件。**

## 1. 不可协商的基线

| 项目 | 要求 |
|---|---|
| 主目标系统 | Windows 7 SP1 x86（Aero 与经典/基本主题都要能启动） |
| 开发验证系统 | Windows 11 x64 |
| 架构 | x86 主构建；不得依赖 x64-only 代码 |
| 工具链 | MSVC v141，Windows SDK 10，`/MT` 静态 CRT |
| PE 子系统 | `WINDOWS,6.01` |
| 运行依赖 | 无 .NET、Java、Electron、Qt、WebView2、VC++ Redistributable |
| 权限 | 普通用户；不安装服务/驱动；不要求管理员 |
| 网络 | 完全离线可用；不发起网络请求 |

## 2. 功能准入流程

任何新功能在写代码前先回答：

1. **所需 API 的最低客户端版本是什么？** 查 Microsoft Learn 的 “Minimum supported client”。
2. **Win7 是否导出该 API？** 不能只看“Windows 7 有同名 DLL”。
3. **是否必须静态导入？** 如果是 Win8+ API，必须改为运行时 `LoadLibrary` + `GetProcAddress`，并提供 Win7 不可用时的降级路径。
4. **降级后功能是否仍可接受？** 如果该功能是核心功能且无法降级，则不能加入。
5. **构建门禁是否通过？** 运行 `scripts\build-win7-x86.cmd`，其中包含 `check-win7-compat.ps1`。
6. **真实 Win7 是否验证？** 至少在一台真实 Win7 SP1 上启动、操作相关路径、退出。未验证时必须在文档中明确标注，不能声称已兼容。

## 3. 禁止的静态导入

以下内容不得出现在 `dumpbin /imports` 中：

- `MAGNIFICATION.dll`（历史问题：`MagShowSystemCursor` 仅 Win8+ 导出）
- `SHCORE.dll`（Win8.1+）
- `DCOMP.dll`（Win8+）
- `D3D11_2.dll`
- `api-ms-win-*` / `ext-ms-win-*` API set
- `VCRUNTIME*` / `MSVCP*` / `ucrtbase.dll`
- 任何 Win8+ 才有的导出，例如：
  - `MagShowSystemCursor`
  - `MagSetFullscreenTransform`
  - `GetDpiForWindow`
  - `SetProcessDpiAwarenessContext`
  - `SetThreadDpiAwarenessContext`
  - `GetSystemMetricsForDpi`
  - `AdjustWindowRectExForDpi`
  - `EnableNonClientDpiScaling`
  - `SetProcessDpiAwareness`
  - `GetDpiForMonitor`
  - `SetCoalescableTimer`
  - `GetPointerType`

如果确实需要新 API，只能通过 `GetProcAddress` 动态调用，并在指针为空时走 Win7 路径。

## 4. 正确做法示例

### 4.1 DPI

已有代码通过动态加载 `shcore.dll!GetDpiForMonitor` 读取每显示器 DPI，失败时回退到系统 DPI。不能直接静态导入 `GetDpiForMonitor`。

### 4.2 视觉/合成

- Win7 Aero 有 DWM；基本主题/关闭 DWM 时要有 timer pacer 回退。
- 逐像素 alpha 优先用分层窗口 + `UpdateLayeredWindow`。
- 圆形、圆角等效果不要依赖 Win8+ 的窗口圆角 API。
- 任务栏、WPS 等置顶弹层的 Z 序修正必须在 Win7 上重新验证。

### 4.3 放大镜教训

`MagShowSystemCursor` 是 Win8+ 导出；Win7 的 `MAGNIFICATION.dll` 没有这个入口。只要 exe 静态导入它，Win7 在加载阶段就会报“无法定位程序输入点”，即使功能从未启用。

该功能已整体移除。以后若要重新引入类似能力，必须动态解析 API 或提供完全独立的 Win7 实现。

## 5. 构建门禁

构建命令：

```bat
scripts\build-win7-x86.cmd
```

它会在复制 exe 后自动运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-win7-compat.ps1 -Exe dist\ZaYuCursorHalo.exe
```

检查内容：

- PE machine 必须是 x86；
- subsystem version 必须 ≤ 6.01；
- 禁止的 DLL / API set / 导出不得出现在静态导入表；
- 检查失败时构建返回非零，不能交付。

手动运行检查：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-win7-compat.ps1 -Exe dist\ZaYuCursorHalo.exe
```

## 6. 发布前 Win7 回归清单

- [ ] 干净 Win7 SP1 x86 虚拟机或真机启动，无“无法定位程序输入点”/缺 DLL 弹窗。
- [ ] 托盘图标、右键面板、设置窗口可打开。
- [ ] 高亮跟随、任务栏上方、WPS 置顶面板。
- [ ] 按键显示与自定义位置。
- [ ] 浮动计时器：开始/暂停/归零/拖动/重启后位置。
- [ ] 开机启动、单实例、退出。
- [ ] Aero 和基本主题各跑一次。
- [ ] 125% / 150% DPI 至少各一次。
- [ ] 空闲 CPU、移动 CPU、工作集无异常回退。
- [ ] 构建门禁输出 PASS。

## 7. 例外处理

如果某个功能确实无法在 Win7 实现，必须：

1. 先向项目所有者说明 API/系统限制；
2. 明确该功能是可选、默认关闭、且不进入静态导入；
3. 在 README 和功能文档中写明 Win7 不支持；
4. 不得让 Win7 用户遇到启动失败或崩溃。

未经确认，不得以“Win7 用户很少”“新系统才有”为理由降低兼容性。

## 8. 当前状态

- 当前 exe 无 `MAGNIFICATION.dll` 静态导入；
- 当前构建门禁 PASS；
- 放大镜功能已移除；
- 字体已从个人使用许可的 Digital-7 替换为 SIL OFL 1.1 的 DSEG7 Modern；
- 最新构建的静态 API 清单见 [WIN7_RELEASE_AUDIT.md](WIN7_RELEASE_AUDIT.md)；
- 最新构建尚未在真实 Windows 7 SP1 上完成回归，发布前必须按第 6 节清单执行；
- 任何后续功能都必须重新跑门禁和 Win7 真机回归。
