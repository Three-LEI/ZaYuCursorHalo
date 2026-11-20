#include "util.h"

#include <shlobj.h>

#include <algorithm>
#include <cstdlib>

double QpcNowSeconds()
{
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value = {};
        QueryPerformanceFrequency(&value);
        return value;
    }();

    LARGE_INTEGER counter = {};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

double QpcNowMilliseconds()
{
    return QpcNowSeconds() * 1000.0;
}

std::wstring GetLocalAppDataDirectory()
{
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw)) && raw)
    {
        std::wstring result(raw);
        CoTaskMemFree(raw);
        return result;
    }
    CoTaskMemFree(raw);

    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (length > 1)
    {
        std::wstring result(length, L'\0');
        DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", result.data(), length);
        result.resize(written);
        return result;
    }
    return L".";
}

std::wstring GetRoamingAppDataDirectory()
{
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &raw)) && raw)
    {
        std::wstring result(raw);
        CoTaskMemFree(raw);
        return result;
    }
    CoTaskMemFree(raw);

    DWORD length = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    if (length > 1)
    {
        std::wstring result(length, L'\0');
        DWORD written = GetEnvironmentVariableW(L"APPDATA", result.data(), length);
        result.resize(written);
        return result;
    }
    return L".";
}

std::wstring GetDefaultStatsPath()
{
    return GetLocalAppDataDirectory() + L"\\CursorHalo\\p0-stats.txt";
}

std::wstring GetParentDirectory(const std::wstring& path)
{
    const std::wstring::size_type pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
        return L".";
    if (pos == 0)
        return path.substr(0, 1);
    return path.substr(0, pos);
}

bool EnsureDirectoryExists(const std::wstring& path)
{
    if (path.empty())
        return false;
    if (CreateDirectoryW(path.c_str(), nullptr))
        return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool EnsureParentDirectoryExists(const std::wstring& path)
{
    return EnsureDirectoryExists(GetParentDirectory(path));
}

std::string NarrowAscii(const std::wstring& text)
{
    std::string result;
    result.reserve(text.size());
    for (wchar_t ch : text)
        result.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    return result;
}
