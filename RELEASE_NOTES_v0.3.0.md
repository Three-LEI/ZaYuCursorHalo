# ZaYuCursorHalo v0.3.0

Windows 7 SP1 x86 优先的原生桌面鼠标高亮 / 键盘显示 / 浮动计时器工具。

## 下载

发布资产：`ZaYuCursorHalo-v0.3.0-win7-x86.zip`

- SHA256: `5873CC63DB8824C2443138927013490CA86E894FA3B1B602D6D4F2D3BB8406EF`
- 解压后直接运行 `ZaYuCursorHalo.exe`，无需安装。
- 无 .NET / Electron / WebView2 / Qt / VC++ Redistributable 依赖。
- 普通用户权限运行，不安装服务或驱动。

## 本版包含

- 鼠标光环：光环 / 双环 / 准星 / 光点。
- 按键显示：4 个基础皮肤、极简主色、位置/延迟、70%–150% 缩放、20%–100% 不透明度。
- 按键快照：以实际键状态为准；IME 和快速输入不会复活旧组合键，同时保留原延迟和淡出动画。
- 自定义位置：半透明 `Ctrl + S` 残影跟随鼠标，预览最终位置。
- 打开设置窗口时，光环、按键、拖尾和点击反馈仍然实时生效。
- 计时器时间码缩小并重新居中；窗口尺寸变化时保持视觉中心。
- 堆叠显示：独立延迟、逐条淡入/滑出；单独按修饰键也会保留一行。
- 浮动计时器：正计时、倒计时、自定义分钟、分段、拖动位置保存。
- 鼠标拖尾、左右键波纹、10 个点击音效预设、0–100% 音量、WAV/MP3 导入。
- 点击音效 PCM 播放串行化并等待 `WHDR_DONE`，短音效补静音，修复快速试听时的 `wdmaud.drv` 崩溃和木纹偶发无声。
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
