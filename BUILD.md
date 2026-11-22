# 构建与测试

本文是仓库中唯一的构建说明，面向想从源码编译 `ZaYuCursorHalo` 的读者。

## 目标

- 目标平台：**Windows 7 SP1 x86**
- 架构：x86（32 位）
- 运行时：`/MT` 静态运行库
- 子系统：`/SUBSYSTEM:WINDOWS,6.01`
- 不依赖 .NET、Electron、WebView2、VC++ Redistributable

## 环境

推荐在 Windows 10/11 上构建，产物面向 Windows 7 SP1：

- Visual Studio 2022 Build Tools
- MSVC **v141** x86 工具集
- Windows SDK
- CMake
- PowerShell 5.1+

构建脚本会自动定位 MSVC、CMake 和 Windows SDK。

## 构建

```bat
git clone https://github.com/Three-LEI/ZaYuCursorHalo.git
cd ZaYuCursorHalo
scripts\build-win7-x86.cmd
```

输出：

```text
dist\ZaYuCursorHalo.exe
```

构建脚本会先重新生成 CMake 工程，再编译并执行 Win7 兼容门禁。

## Win7 门禁

门禁脚本：

```text
scripts\check-win7-compat.ps1
```

检查内容：

- PE 为 x86；
- `SUBSYSTEM` ≤ 6.01；
- 无 Win8+ API / API set 静态导入；
- 无 UCRT / VC++ 运行库静态依赖；
- 无 `MAGNIFICATION.dll` 等已知不兼容导入。

门禁失败时构建失败。

## 冒烟测试

构建后运行：

```powershell
tools\p0-smoke.ps1 -Exe dist\ZaYuCursorHalo.exe
tools\p1-settings-smoke.ps1 -Exe dist\ZaYuCursorHalo.exe
```

## 打包

```powershell
scripts\package-release.ps1 -Version 0.3.0
```

生成：

```text
release\ZaYuCursorHalo-v0.3.0-win7-x86.zip
```

## 目录

```text
src\            C++ / Win32 / Direct2D 源码
res\            图标、字体、音效资源
third_party\    第三方许可证与源码说明
scripts\        构建、门禁、打包脚本
tools\          冒烟测试与验证工具
artifacts\      README 截图
```

## Win7 说明

- 门禁 PASS 只证明 PE 结构和静态导入没有明显 Win8+ 依赖；
- 仍建议在干净 Windows 7 SP1 x86/x64 上解压启动，确认无缺少 DLL / 无法定位入口点；
- 真机结果请以 GitHub Release Notes 为准。

## 许可证

项目使用 MIT License，第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
