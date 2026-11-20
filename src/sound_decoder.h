#pragma once

#include <string>

// Decodes an MP3 into a 16-bit PCM WAV using the bundled minimp3 decoder.
// Used only by the import path; playback stays PlaySound-compatible.
bool DecodeMp3ToWav(
    const std::wstring& sourcePath,
    const std::wstring& destinationPath,
    std::wstring& error);
