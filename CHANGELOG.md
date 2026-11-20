# Changelog

## v0.3.0 — 2026-11-20

### Added

- Click sound volume (0–100%) with per-sample PCM scaling for presets and custom WAVs.
- PCM previews are serialized and wait for `WHDR_DONE`, fixing rapid slider preview crashes in `wdmaud.drv`.
- Short click sounds get silence padding and waveOut retry/fallback, fixing intermittent silent wood previews.
- IME key-ups no longer restart the fade delay, so old hints cannot freeze half-faded while typing.
- Key display rebuilt around an immutable chord snapshot plus `GetAsyncKeyState` reconciliation; filtered typing keeps the original delay/fade instead of rebuilding stale keys or resetting the timer.
- Stack mode gives modifier-only rows the normal stack delay and fade instead of dropping Ctrl/Shift instantly.
- Mouse halo with ring, double-ring, crosshair and dot styles.
- Global keyboard display with 4 built-in skins and a minimal main color.
- Key display scale (70%–150%) and opacity (20%–100%), both inside the preview card.
- Settings window no longer suspends halo, key display, trail or click feedback.
- Timer digits use a smaller DSEG7 size with optical centering; capsule size changes keep the visual center stable.
- Custom-position placement now shows a translucent keyboard ghost for previewing.
- Key stack display with independent delay and enter/exit slide/fade animations.
- Floating timer: count-up/countdown, custom 1–99 minutes, segments and last split.
- Mouse trail, left/right click ripples and ten click-sound presets.
- WAV/MP3 click sound import; MP3 is decoded locally to PCM WAV.
- Tray panel, settings window, hotkey recording, theme and start-with-Windows.
- Settings window appears in the taskbar and Alt+Tab list.
- Taskbar-aware overlay ordering and click-through layered windows.

### Fixed

- Key hints now replace old ordinary keys while fast typing instead of
  accumulating stale chords.
- Filtered letter keys keep the existing hint's delay and fade-out animation
  instead of hiding immediately.
- HEX color input clamps the caret and handles numpad digits safely.
- Appearance color swatches no longer draw a second "selected" ring around the
  custom-color button.

### Compatibility

- Windows 7 SP1 x86 primary target.
- MSVC v141, `/MT`, `/SUBSYSTEM:WINDOWS,6.01`.
- Build gate checks x86, subsystem <= 6.01, and forbidden static imports.
- Win7 runtime validation is listed separately in
  `docs/WIN7_RELEASE_AUDIT.md`; the current CI/host smoke test is on Windows 11.
