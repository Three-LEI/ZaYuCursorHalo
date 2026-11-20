# CursorHalo v0.3.0

Windows 7 SP1 x86 优先的原生桌面鼠标高亮 / 键盘显示 / 浮动计时器工具。

## 下载

发布资产：`CursorHalo-v0.3.0-win7-x86.zip`

- SHA256: `4FC84C2964C7C2DDFB1452155351CC03825C82B0466C9818C6B4D9E9D4B3EC9E`
- 解压后直接运行 `CursorHalo.exe`，无需安装。
- 无 .NET / Electron / WebView2 / Qt / VC++ Redistributable 依赖。
- 普通用户权限运行，不安装服务或驱动。

## 本版包含

- 鼠标光环：光环 / 双环 / 准星 / 光点。
- 按键显示：4 个基础皮肤、极简主色、位置/延迟、连续输入不累积、延迟淡出。
- 堆叠显示：独立延迟、逐条淡入/滑出。
- 浮动计时器：正计时、倒计时、自定义分钟、分段、拖动位置保存。
- 鼠标拖尾、左右键波纹、10 个点击音效预设、WAV/MP3 导入。
- 托盘面板、设置窗口、任务栏/Alt+Tab、全局快捷键、主题、开机启动。

## Win7 兼容状态

- x86，`/MT`，`SUBSYSTEM 6.01`。
- Win7 构建门禁 PASS：无 Win8+ API、API set、UCRT、VCRuntime、DComp、Magnification 静态导入。
- 静态 API 清单：[docs/WIN7_RELEASE_AUDIT.md](docs/WIN7_RELEASE_AUDIT.md)。
- **本版尚未在真实 Windows 7 SP1 机器上完成回归**；当前只在 Windows 11 上通过构建、门禁和冒烟测试。
- 发布前请按 [docs/win7-compatibility.md](docs/win7-compatibility.md) 第 6 节在干净 Win7 SP1 x86/x64 上逐项确认；未实测不建议在发布说明中写“绝对兼容”。

## License

MIT。第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

制作：**three-lei × DeepSeek**
