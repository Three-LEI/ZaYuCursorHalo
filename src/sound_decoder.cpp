#include "sound_decoder.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>

#define MINIMP3_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244 4267 4456 4457 4459 4706)
#include "../third_party/minimp3/minimp3_ex.h"
#pragma warning(pop)

namespace
{
void WriteLE16(BYTE*& cursor, unsigned value)
{
    *cursor++ = static_cast<BYTE>(value & 0xFF);
    *cursor++ = static_cast<BYTE>((value >> 8) & 0xFF);
}

void WriteLE32(BYTE*& cursor, unsigned value)
{
    *cursor++ = static_cast<BYTE>(value & 0xFF);
    *cursor++ = static_cast<BYTE>((value >> 8) & 0xFF);
    *cursor++ = static_cast<BYTE>((value >> 16) & 0xFF);
    *cursor++ = static_cast<BYTE>((value >> 24) & 0xFF);
}

bool WriteWav(
    const std::wstring& path,
    const mp3d_sample_t* samples,
    size_t sampleCount,
    int channels,
    int sampleRate)
{
    const size_t dataBytes = sampleCount * sizeof(mp3d_sample_t);
    if (dataBytes > 0xFFFFFFFFull - 44)
        return false;

    BYTE header[44] = {};
    BYTE* cursor = header;
    std::memcpy(cursor, "RIFF", 4);
    cursor += 4;
    WriteLE32(cursor, static_cast<unsigned>(36 + dataBytes));
    std::memcpy(cursor, "WAVEfmt ", 8);
    cursor += 8;
    WriteLE32(cursor, 16);
    WriteLE16(cursor, 1);
    WriteLE16(cursor, static_cast<unsigned>(channels));
    WriteLE32(cursor, static_cast<unsigned>(sampleRate));
    WriteLE32(cursor, static_cast<unsigned>(sampleRate * channels * sizeof(mp3d_sample_t)));
    WriteLE16(cursor, static_cast<unsigned>(channels * sizeof(mp3d_sample_t)));
    WriteLE16(cursor, 16);
    std::memcpy(cursor, "data", 4);
    cursor += 4;
    WriteLE32(cursor, static_cast<unsigned>(dataBytes));

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const BOOL headerOk = WriteFile(file, header, sizeof(header), &written, nullptr) &&
        written == sizeof(header);
    const BOOL dataOk = headerOk && WriteFile(
        file,
        samples,
        static_cast<DWORD>(dataBytes),
        &written,
        nullptr) &&
        written == dataBytes;
    CloseHandle(file);
    if (!headerOk || !dataOk)
    {
        DeleteFileW(path.c_str());
        return false;
    }
    return true;
}
} // namespace

bool DecodeMp3ToWav(
    const std::wstring& sourcePath,
    const std::wstring& destinationPath,
    std::wstring& error)
{
    mp3dec_t decoder = {};
    mp3dec_file_info_t info = {};
    if (mp3dec_load_w(&decoder, sourcePath.c_str(), &info, nullptr, nullptr) != 0 ||
        !info.buffer || info.samples == 0 || info.hz <= 0 ||
        (info.channels != 1 && info.channels != 2))
    {
        if (info.buffer)
            free(info.buffer);
        error = L"无法解码这个 MP3，文件可能已损坏或不是有效的 MP3。";
        return false;
    }

    const double seconds =
        static_cast<double>(info.samples) /
        (static_cast<double>(info.hz) * static_cast<double>(info.channels));
    if (seconds > 15.0)
    {
        free(info.buffer);
        error = L"点击音效请控制在 15 秒以内。";
        return false;
    }

    const bool ok = WriteWav(
        destinationPath,
        info.buffer,
        info.samples,
        info.channels,
        info.hz);
    free(info.buffer);
    if (!ok)
    {
        error = L"无法写入转换后的 WAV 文件，请检查配置目录权限。";
        return false;
    }
    return true;
}
