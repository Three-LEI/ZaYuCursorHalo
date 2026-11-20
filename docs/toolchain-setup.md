# Windows 7 目标工具链安装记录

> 安装时间：2026-11-19  
> 目标：用 v141 + Windows SDK 10.0.19041 构建可在 Windows 7 SP1 上运行的 C++ Win32/Direct2D 程序。  
> 构建后自动执行 Win7 兼容门禁，见 [win7-compatibility.md](win7-compatibility.md)。

## 已安装组件

| 组件 | 版本 | 路径 |
|---|---|---|
| Visual Studio 2022 Build Tools | 17.14.45 | `F:\DevTools\VS2022\BuildTools` |
| MSVC v141 工具集 | 14.16.27023（cl 19.16.27054.0） | `F:\DevTools\VS2022\BuildTools\VC\Tools\MSVC\14.16.27023` |
| Windows SDK | 10.1.19041.5609，头/库版本 10.0.19041.0 | `D:\Windows Kits\10` |
| CMake（VS 自带） | 3.31.6-msvc6 | `F:\DevTools\VS2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| Ninja（VS 自带） | 1.12.1 | `F:\DevTools\VS2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe` |

VS 2026 仍保留在 `D:\Program Files\Microsoft Visual Studio\18\Community`。项目发布构建必须显式选择 v141，不能依赖 VS 默认工具集。

## 为什么 SDK 19041 在 D 盘而不是 F 盘

Windows SDK 安装器不允许第二个 SDK 根目录。本机已有 SDK 26100，根目录是：

```text
D:\Windows Kits\10
```

直接指定 `/installpath F:\...` 会被拒绝（退出码 1001，日志明确提示只能装到已选定的根目录）。因此 19041 被安装到同一根目录下，与 26100 按版本共存：

```text
D:\Windows Kits\10\Include\10.0.19041.0
D:\Windows Kits\10\Include\10.0.26100.0
D:\Windows Kits\10\Lib\10.0.19041.0
D:\Windows Kits\10\Lib\10.0.26100.0
```

没有向 C 盘安装 SDK 产品文件。C 盘只出现了安装器缓存（见下文）。

## 构建命令

### 直接使用 vcvarsall

```bat
call "F:\DevTools\VS2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 10.0.19041.0 -vcvars_ver=14.16
```

- 必须带上 `-vcvars_ver=14.16`，否则会选中机器上已有的 v143（14.44）。
- 必须带上 `10.0.19041.0`，否则可能默认选到 26100 SDK。
- 编译参数至少包含：

```bat
/DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DNTDDI_VERSION=0x06010000
/MT
/SUBSYSTEM:WINDOWS,6.01
```

### CMake

```bat
"F:\DevTools\VS2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . -B build ^
  -G "Visual Studio 17 2022" -A Win32 -T v141 ^
  -DCMAKE_SYSTEM_VERSION=10.0.19041.0
```

项目需在 CMake 中设置：

```cmake
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
```

验证生成的 `.vcxproj` 应包含：

```xml
<PlatformToolset>v141</PlatformToolset>
<WindowsTargetPlatformVersion>10.0.19041.0</WindowsTargetPlatformVersion>
<RuntimeLibrary>MultiThreaded</RuntimeLibrary>
```

## 实际编译验证

在 `F:\DevTools\Verify` 建了一个最小 D2D + DirectWrite 程序，包含自定义 manifest 和 D2D/DWM 调用，分别用直接 cl 和 CMake 构建。

结果：

- x86/x64 编译、链接成功。
- x86 PE：`machine (x86)`、`subsystem version 6.01`、`operating system version 6.01`、linker 14.16。
- x64 PE：`machine (x64)`、`subsystem version 6.01`、linker 14.16。
- 运行时在 Win11 上退出码 0。
- 导入表仅有：`KERNEL32.dll`、`USER32.dll`、`ole32.dll`、`d2d1.dll`、`DWrite.dll`、`dwmapi.dll`。
- 没有 `VCRUNTIME140.dll`/`MSVCP140.dll`/`ucrtbase.dll`/`api-ms-win-*` 动态依赖，确认 `/MT` 静态 CRT 生效。
- 没有出现 `GetDpiForWindow`、`SetProcessDpiAwarenessContext` 等 Win8+ API。
- CMake 配置实际选择：`CMAKE_GENERATOR_TOOLSET=v141`、`WindowsTargetPlatformVersion=10.0.19041.0`、`RuntimeLibrary=MultiThreaded`。

### 自定义 manifest 的一个坑

如果 `.rc` 文件已经内嵌了一个 `RT_MANIFEST`，而 CMake/MSBuild 又自动生成 manifest，链接会出现：

```text
CVTRES : fatal error CVT1100: 资源重复。类型: MANIFEST，名称: 1
```

解决：项目使用自定义 manifest 时，在链接选项加：

```cmake
target_link_options(target PRIVATE /MANIFEST:NO)
```

这正是验证程序里修复的问题，后续正式工程直接沿用。

## C 盘占用说明

产品和 SDK 文件没有安装到 C 盘，但安装过程在 C 盘产生了安装器缓存：

- `C:\ProgramData\Package Cache` 下新增的 SDK 19041 MSI 缓存：约 **298 MB**
- `C:\ProgramData\Microsoft\VisualStudio\Packages\_bootstrapper`：约 **65.5 MB**
- 另含 VS Installer 自更新等少量文件

C 盘可用空间从约 15.22 GB 变为 14.47 GB。上述可识别缓存合计约 0.36 GB。如需回收，可将这些 SDK 包缓存迁到 F 盘并用目录联接（junction）保留原路径；这会改动官方修复/卸载缓存，建议单独确认后再做。

## 安装过程中处理过的问题

1. VS BuildTools 自更新通过本机系统代理（Verio/mihomo `127.0.0.1:56189`）下载 68 MB 的 `vs_installer.opc` 时会卡住；安装时临时关闭 WinINET 代理后成功，结束后已恢复代理设置。
2. 第一次自更新失败是因为并行的手动下载占用了同名 `.opc` 文件，清理后恢复。
3. SDK 不能使用第二个根目录，改为安装到现有 `D:\Windows Kits\10`。
4. CMake 自定义 manifest 与自动生成 manifest 冲突，使用 `/MANIFEST:NO` 解决。

## 尚未验证

- 没有真实 Windows 7 SP1 机器/虚拟机，尚未做 Win7 运行时启动测试。
- 当前只验证了“目标 PE 版本 6.01 + 只导入 Win7 自带 DLL + 静态 CRT”这一静态兼容性基础；最终仍需在 Win7 SP1 x86/x64 的 Aero/基本主题环境实机验收。

## 下一步

1. 以该工具链建立最小的 `ZaYuCursorHalo` P0 穿刺程序（分层窗口 + D2D 光环 + 托盘 + 热键）。
2. 在 Win11 上先跑通功能与帧率。
3. 拿到 Win7 环境后按 [technical-roadmap.md](technical-roadmap.md) 的兼容门禁逐项验收。
