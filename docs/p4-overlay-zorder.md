# P4 高亮层 Z 序

> 适用版本：ZaYuCursorHalo v0.3.0

## 目标

- 光环高于普通应用窗口；
- 高于 WPS 等应用的置顶弹层；
- 保持在任务栏上方，不遮住托盘图标；
- 点击直接穿透，不影响任务栏和其他程序。

![任务栏上方](../artifacts/p4-overlay-taskbar.png)

![WPS 置顶场景](../artifacts/p4-overlay-wps.png)

## 实现

- 置顶分层窗口 + `UpdateLayeredWindow` 逐像素 alpha；
- `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW`；
- 不进入 Alt+Tab，不抢焦点；
- 工作区夹取，避开任务栏；
- 收到任务栏重建/显示变化消息后重新校正 Z 序；
- 设置窗口和托盘面板打开时，光环仍显示并保持点击穿透。

## 验证

| 项目 | 结果 |
|---|---|
| 普通窗口 | 光环在最上层 |
| WPS 置顶弹层 | 光环仍可见 |
| 任务栏 | 不被遮挡，点击穿透 |
| 设置/托盘面板 | 光环可见且不抢点击 |

真实 Win7 Aero/Basic 下仍需复测 Z 序和任务栏区域。
