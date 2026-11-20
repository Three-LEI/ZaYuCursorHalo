#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <vector>

class FrameStats
{
public:
    FrameStats();

    void Reset();
    void Record(double intervalMs, double renderMs, double presentMs, double pacerWaitMs);
    void SetRunInfo(double refreshHz, const wchar_t* pacerMode, bool compositionEnabled, int radiusDip, bool benchmark);

    std::size_t frameCount() const { return samples_.size(); }
    bool WriteSummary(const std::wstring& path) const;

private:
    struct Sample
    {
        double intervalMs = 0.0;
        double renderMs = 0.0;
        double presentMs = 0.0;
        double pacerWaitMs = 0.0;
    };

    std::vector<Sample> samples_;
    double refreshHz_ = 0.0;
    std::wstring pacerMode_ = L"unknown";
    bool compositionEnabled_ = false;
    int radiusDip_ = 0;
    bool benchmark_ = false;
};
