# P5 点击反馈与光环样式

> 适用版本：ZaYuCursorHalo v0.3.0  
> exe：847,872 B

## 点击波纹

- 左键、右键分别设置颜色（HSV/HEX）；
- 波纹在点击位置扩散并淡出；
- 颜色即时生效并持久化。

![点击波纹颜色](../artifacts/p5-click-ripple-color.png)

## 点击音效

侧边栏“点击”页提供 5 组预设：

- 默认；
- 柔和；
- 清脆；
- 木纹；
- 合成。

![点击音效设置](../artifacts/p5-click-volume.png)

- 点击预设立即试听；
- 音量 0–100%，默认 100%；
- 支持导入 WAV/MP3；
- WAV 原样复制到 `%APPDATA%\ZaYuCursorHalo\sounds`；
- MP3 使用内置 minimp3 解码为 16-bit PCM WAV，最长 15 秒；
- 导入后自动切换到“自定义”，左/右键共用；
- 再次导入会替换自定义文件；内置预设仍可随时切回。

```ini
[effects]
click_ripple=0
click_sound=0
volume=100
left_color=5B8DFF
right_color=FF6B6B
sound_preset=crisp
sound_file=
```

## 光环样式

设置侧边栏“外观”页：

- 光环；
- 双环；
- 准星；
- 光点。

![光环样式设置](../artifacts/p4-halo-styles-settings.png)

## 音效实现

- 内置 WAV 以 `WAVE` 资源嵌入 exe；
- 音量 100%：`PlaySoundW` 直接播放；
- 音量 0–99%：解析 PCM，按音量缩放采样后通过 `waveOut` 播放；
- PCM 播放串行化，等待 `WHDR_DONE` 后释放缓冲，避免快速试听时驱动访问已释放内存；
- 短音效自动补 60 ms 首尾静音，避免 40 ms 木纹音效被 WDM 启动延迟吞掉；
- `waveOutOpen` 失败重试 3 次，仍失败则回退到 `PlaySound`；
- 自定义 WAV：同样按 PCM 缩放；不支持的编码回退 `PlaySound`/MCI；
- MP3 不做运行时解码，导入时转成 16-bit PCM WAV。

## 验证

| 项目 | 结果 |
|---|---|
| 10 个预设资源 | 全部可加载/播放 |
| 清脆预设 | 55 ms，已提高振幅和时长 |
| WAV 导入 | 复制并可播放 |
| MP3 导入 | minimp3 解码，最长 15 秒 |
| 左右键颜色 | 独立生效并持久化 |
| 音量 | 0%/50%/100% 采样缩放，配置持久化 |
| 4 种光环样式 | 设置页即时切换 |
