# ZaYuCursorHalo Win7 release audit

Date: 2026-11-20  
Build: `v0.3.0` (0.3.0.0), x86  
Target: Windows 7 SP1 x86/x64

## 1. Build result

| Check | Result |
|---|---|
| Toolset | MSVC v141 (VS 2017 toolset), CMake VS 2022 generator |
| Architecture | x86 (`machine (x86)`) |
| Runtime | `/MT` static CRT |
| Subsystem | `WINDOWS,6.01` |
| Static imports | No `MAGNIFICATION`, `SHCORE`, `DCOMP`, `D3D11_2`, `api-ms-win-*`, `ext-ms-win-*`, `VCRUNTIME*`, `MSVCP*`, `ucrtbase` |
| Win7 gate | PASS |

The gate is executed automatically by:

```bat
scripts\build-win7-x86.cmd
```

and can be run manually:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-win7-compat.ps1 -Exe dist\ZaYuCursorHalo.exe
```

## 2. Static API surface

Release imports:

`ADVAPI32.dll, COMDLG32.dll, d2d1.dll, dwmapi.dll, DWrite.dll, GDI32.dll,
gdiplus.dll, IMM32.dll, KERNEL32.dll, ole32.dll, SHELL32.dll, USER32.dll,
WINMM.dll`

Win7 availability notes:

- `d2d1.dll` — `D2D1CreateFactory` is imported as ordinal 1. Direct2D 1.0 is
  available on Windows 7 (and Vista SP2 with the platform update).
- `DWrite.dll` — DirectWrite 1.0; Windows 7.
- `dwmapi.dll` — `DwmIsCompositionEnabled`, `DwmGetCompositionTimingInfo`,
  `DwmFlush`; Windows 7. Basic theme / DWM-off falls back to the timer pacer.
- `gdiplus.dll` — GDI+ 1.1 APIs, including private memory fonts; Windows 7.
- `SHELL32.dll` — `Shell_NotifyIconGetRect` requires Windows 7 (exact target),
  `SHGetKnownFolderPath` requires Vista.
- `WINMM.dll` — `PlaySoundW`, `mciSendStringW`; Windows 7.
- `KERNEL32.dll` — all imported functions are Vista/7-era or older, including
  `GetTickCount64`, `CreateWaitableTimerExW`, `InitializeCriticalSectionAndSpinCount`.
- `IMM32.dll` — `ImmAssociateContext`; Windows 7.
- `USER32/GDI32/ADVAPI32/OLE32/COMDLG32` — only long-standing desktop APIs.

No Win8+ API is present in the static import table.

## 3. Runtime-loaded APIs

These are loaded with `LoadLibraryW` + `GetProcAddress`, so a missing symbol on
Windows 7 does not prevent startup:

- `shcore.dll!GetDpiForMonitor` — absent on Win7; falls back to device caps.
- `user32.dll!GetDpiForWindow` — absent on Win7; falls back to monitor/device caps.

The Win7 fallback paths are exercised by code review and by the Windows 11
build, but they still need a true Windows 7 run.

## 4. Manifest

- `asInvoker`, no admin prompt.
- `dpiAware=true` under the Windows 2005 settings namespace (Win7-supported).
- `dpiAwareness=PerMonitorV2,PerMonitor` under the Windows 2016 namespace is
  ignored by Windows 7; modern Windows uses it.
- `supportedOS` includes Vista, Windows 7, 8, and 8.1/10 identities.
- No DLL/assembly dependencies.

## 5. Assets

- DSEG7 Modern font is SIL OFL 1.1; license in
  `third_party/dseg/DSEG-LICENSE.txt`.
- minimp3 is CC0; notice in `third_party/minimp3/README.md`.
- Click WAVs and icons are project assets.
- No UCRT, VCRuntime, .NET, Java, Electron, Qt, WebView2 or installer dependency.

## 6. What was actually tested here

Passed on the build host (Windows 11 x64, 32-bit exe):

- Build + Win7 compatibility gate.
- Smoke test: startup, tray, hotkey hide/show, overlay hit-through, idle CPU,
  working set.
- Manual screenshots for settings, key display, stack, timer, click feedback,
  skin/color states.
- HEX-input caret/numpad stress test.
- Taskbar/Alt+Tab style (`WS_EX_APPWINDOW`) inspection.

**Not tested here:** a real Windows 7 SP1 machine or VM. Therefore this document
does not claim "absolutely verified on Win7"; it records the static/build
evidence. Before publishing, run the checklist in
`docs/win7-compatibility.md` on a clean Win7 SP1 x86 or x64 machine.

## 7. Required final Win7 checklist

- [ ] Clean Win7 SP1 x86 VM or physical machine; no VC++ redistributable.
- [ ] Launch from a folder with a non-ASCII path and from Program Files.
- [ ] No "无法定位程序输入点" or missing-DLL dialog.
- [ ] Tray icon, left-click toggle, right-click panel, Settings.
- [ ] Halo follows cursor and stays above taskbar/WPS topmost panels.
- [ ] Key display, 4 skins, stack delay and fade animations.
- [ ] Timer start/pause/reset/drag/restart position; DSEG7 digits.
- [ ] Trail, click ripple, both click sound channels and custom WAV/MP3.
- [ ] Aero and Basic themes; 100%/125%/150% DPI.
- [ ] Sleep/resume, display change, multi-monitor, single-instance.
- [ ] `scripts\check-win7-compat.ps1` PASS on the release exe.
- [ ] Record the result in the release notes.

If any item fails, do not publish that build as Win7-compatible.
