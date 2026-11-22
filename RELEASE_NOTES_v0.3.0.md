# ZaYuCursorHalo v0.3.0

Windows 7 SP1 x86 优先的原生桌面鼠标高亮 / 键盘显示 / 浮动计时器工具。

## 下载

发布资产：`ZaYuCursorHalo-v0.3.0-win7-x86.zip`

- SHA256: `BD1D69033A292792EB78D9D28B088EA9DF58C6925121A2ADB2E23B383E035E17`
- 解压后直接运行 `ZaYuCursorHalo.exe`，无需安装。
- 无 .NET / Electron / WebView2 / Qt / VC++ Redistributable 依赖。
- 普通用户权限运行，不安装服务或驱动。

## 本版包含

- 鼠标光环：光环 / 双环 / 准星 / 光点。
- 按键显示：4 个基础皮肤、极简主色、位置/延迟、70%–150% 缩放、20%–100% 不透明度。
- 按键快照：以实际键状态为准；IME 和快速输入不会复活旧组合键，同时保留原延迟和淡出动画。
- 跟随鼠标：首次显示时记录鼠标位置，固定显示在下方；之后移动鼠标不会拖动已显示的按键。
- 堆叠显示：每行先向下缓出，空位收起、下一行完全上移后才执行自己的缓出；关闭堆叠时单行恢复向下缓出。
- 自定义位置：半透明 `Ctrl + S` 残影跟随鼠标，预览最终位置。
- 打开设置窗口时，光环、按键、拖尾和点击反馈仍然实时生效。
- 计时器时间码缩小并重新居中；窗口尺寸变化时保持视觉中心。
- 浮动计时器：正计时、倒计时、自定义分钟、分段、拖动位置保存。
- 鼠标拖尾：独立开关，不依赖高亮；可调长度、停留时间、曲线平滑度、宽度、不透明度、发光，颜色支持自动/自定义；末端圆点随轨迹淡出。
- 拖尾页支持重置拖尾设置，通用页支持一键重置所有选项。
- 左右键波纹、10 个点击音效预设、0–100% 音量、WAV/MP3 导入。
- 点击音效 PCM 播放串行化并等待 `WHDR_DONE`，短音效补静音，修复快速试听时的 `wdmaud.drv` 崩溃和木纹偶发无声。
- 关于页：开源仓库、Releases、版本、许可证、第三方组件和作者主页按钮。
- 设置侧边栏共 9 页，README 提供全部页面的默认蓝色主题预览。
- 托盘面板、设置窗口、任务栏/Alt+Tab、全局快捷键、主题、开机启动。

## Win7 兼容状态

- x86，`/MT`，`SUBSYSTEM 6.01`。
- Win7 构建门禁 PASS：无 Win8+ API、API set、UCRT、VCRuntime、DComp、Magnification 静态导入。
- 静态 API 清单：[docs/WIN7_RELEASE_AUDIT.md](docs/WIN7_RELEASE_AUDIT.md)。
- 此前上传的 v0.3.0 构建（含音量与按键快照修复）已在 **Windows 7 SP1 真机**完成基础功能验证，运行正常。
- 本次新增的拖尾设置页、堆叠串行动画、重置功能和关于页尚未在 Windows 7 SP1 真机回归。
- 发布前请按 [docs/win7-compatibility.md](docs/win7-compatibility.md) 第 6 节在干净 Win7 SP1 x86/x64 上逐项确认；未实测不建议在发布说明中写“绝对兼容”。

## License

MIT。第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

制作：**three-lei × DeepSeek**
