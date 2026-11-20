# GitHub 发布步骤

本地仓库已经初始化为 `main`，并创建了 `v0.3.0` tag。当前没有配置 remote，
所以还没有推送到 GitHub。

## 1. 创建空仓库

在 GitHub 新建一个空仓库（不要勾选 README/LICENSE），例如：

- Name: `ZaYuCursorHalo`
- Description: `Win7-friendly native mouse highlight, key display and floating timer`
- Topics: `windows-7`, `win32`, `direct2d`, `mouse-highlight`, `keyboard-visualizer`

## 2. 配置 remote 并推送

```powershell
git remote add origin https://github.com/<your-name>/ZaYuCursorHalo.git
git push -u origin main
git push origin v0.3.0
```

## 3. 上传 Release 资产

- Tag: `v0.3.0`
- Title: `ZaYuCursorHalo v0.3.0`
- Body: 粘贴 [`RELEASE_NOTES_v0.3.0.md`](../RELEASE_NOTES_v0.3.0.md)
- Asset: `release/ZaYuCursorHalo-v0.3.0-win7-x86.zip`
- SHA256: 见 [`RELEASE_NOTES_v0.3.0.md`](../RELEASE_NOTES_v0.3.0.md)（发布时以实际文件哈希为准）

`release/` 不会进入 git（已忽略），只作为 Release 附件上传。

## 4. 发布前最后一道 Win7 真机检查

在干净 Windows 7 SP1 x86 或 x64 上解压 Release zip，按
[`win7-compatibility.md`](win7-compatibility.md) 第 6 节和
[`WIN7_RELEASE_AUDIT.md`](WIN7_RELEASE_AUDIT.md) 第 7 节逐项确认。

只有真机通过后，才把 Release Notes 里的“尚未完成真机回归”改成
“Windows 7 SP1 真机验证通过”。当前构建门禁 PASS 不等于真机绝对兼容。

## 5. 如果不打 tag 先试运行

```powershell
scripts\build-win7-x86.cmd
tools\p0-smoke.ps1 -Exe dist\ZaYuCursorHalo.exe
```
