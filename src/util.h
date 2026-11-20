#pragma once

#include <windows.h>
#include <string>

double QpcNowSeconds();
double QpcNowMilliseconds();

std::wstring GetLocalAppDataDirectory();
std::wstring GetRoamingAppDataDirectory();
std::wstring GetDefaultStatsPath();
std::wstring GetParentDirectory(const std::wstring& path);
bool EnsureDirectoryExists(const std::wstring& path);
bool EnsureParentDirectoryExists(const std::wstring& path);

std::string NarrowAscii(const std::wstring& text);
