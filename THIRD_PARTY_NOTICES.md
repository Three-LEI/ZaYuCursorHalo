# Third-party notices

CursorHalo is MIT licensed. The following third-party components are included
in the source tree or embedded in the release executable.

## minimp3

- Files: `third_party/minimp3/minimp3.h`, `third_party/minimp3/minimp3_ex.h`
- Copyright: Martin Fiedler and contributors
- License: CC0 1.0 Universal (public domain dedication)
- Purpose: decode user-imported MP3 click sounds to PCM WAV locally.
- Upstream: https://github.com/lieff/minimp3

The CC0 notice is also present in the minimp3 headers.

## DSEG7 Modern font

- File: `res/dseg7-modern.ttf` (DSEG7 Modern Regular, v0.46)
- Copyright: 2017-2020 keshikan
- License: SIL Open Font License, Version 1.1
- Full license: `third_party/dseg/DSEG-LICENSE.txt`
- Upstream: https://github.com/keshikan/DSEG
- Purpose: seven-segment timer digits; embedded as `IDR_TIMER_FONT` RCDATA.

## Windows system components

The release executable statically imports only Windows system DLLs (Direct2D,
DirectWrite, GDI+, DWM, User32, GDI32, Shell32, Comdlg32, Ole32, Advapi32,
WinMM, Imm32). No Microsoft runtime redistributable is bundled.

The build uses the MSVC v141 toolset and the Windows SDK headers/libraries;
those are build-time tools and are not redistributed in this repository.
