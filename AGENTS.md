# ZaYuCursorHalo 工程规则

## 最高优先级

**Windows 7 SP1 x86 是本项目的第一目标。任何新功能、重构、依赖升级，都必须先通过 Win7 兼容性评估，再进入实现和发布。**

开始任何功能前，先读：

- [`docs/win7-compatibility.md`](docs/win7-compatibility.md)
- [`docs/technical-roadmap.md`](docs/technical-roadmap.md)

## 强制要求

1. 不得静态导入 Win8+ API、API set、UCRT 或 VC++ Redistributable。
2. 需要新系统 API 时，使用 `LoadLibrary` + `GetProcAddress`，并实现 Win7 降级路径。
3. 保持：x86、MSVC v141、`/MT`、`/SUBSYSTEM:WINDOWS,6.01`。
4. 构建必须使用：

   ```bat
   scripts\build-win7-x86.cmd
   ```

   该脚本内置 `scripts/check-win7-compat.ps1` 门禁；检查失败视为构建失败。

5. 不得重新引入 `MAGNIFICATION.dll` 静态导入；历史上 `MagShowSystemCursor` 导致 Win7 启动即报“无法定位程序输入点”。
6. 新增功能必须更新 README/相关文档，并明确标注真实 Win7 验证状态；未实测不能写“已兼容”。
7. 修改后运行冒烟测试：

   ```powershell
   tools\p0-smoke.ps1 -Exe dist\ZaYuCursorHalo.exe
   ```

8. 保留署名：`three-lei × DeepSeek`。

## 兼容性判断顺序

1. 查 API Minimum supported client。
2. 查 Win7 是否导出该符号。
3. 能动态加载就动态加载；不能就降级或拒绝功能。
4. 跑构建门禁。
5. 在真实 Win7 SP1 上启动并跑相关路径。
6. 更新文档和截图。

## 禁止的捷径

- 不因为“Win7 用户少”而降低兼容性。
- 不把 Win8+ 功能默认开启后再做条件判断。
- 不把“代码看起来能编译”当作 Win7 兼容验证。
- 不在没有 Win7 实测的情况下宣称已兼容。
