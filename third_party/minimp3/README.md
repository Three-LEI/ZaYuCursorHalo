# minimp3

Vendored from https://github.com/lieff/minimp3 for MP3-to-WAV decoding during
click-sound import.

License: CC0 1.0 public domain dedication. See the header comments in
`minimp3.h` and `minimp3_ex.h`.

Files:

- `minimp3.h`
- `minimp3_ex.h`

Only `src/sound_decoder.cpp` defines `MINIMP3_IMPLEMENTATION`, so the decoder
is compiled once.
