#include "frame_stats.h"

#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace
{
double Percentile(const std::vector<double>& sorted, double percentile)
{
    if (sorted.empty())
        return 0.0;
    if (percentile <= 0.0)
        return sorted.front();
    if (percentile >= 100.0)
        return sorted.back();
    const double index = (percentile / 100.0) * static_cast<double>(sorted.size() - 1);
    const std::size_t low = static_cast<std::size_t>(std::floor(index));
    const std::size_t high = static_cast<std::size_t>(std::ceil(index));
    if (low == high)
        return sorted[low];
    const double fraction = index - static_cast<double>(low);
    return sorted[low] + (sorted[high] - sorted[low]) * fraction;
}

} // namespace

FrameStats::FrameStats()
{
    samples_.reserve(240000);
}

void FrameStats::Reset()
{
    samples_.clear();
}

void FrameStats::Record(double intervalMs, double renderMs, double presentMs, double pacerWaitMs)
{
    if (samples_.size() >= samples_.capacity())
        return;
    samples_.push_back(Sample{intervalMs, renderMs, presentMs, pacerWaitMs});
}

void FrameStats::SetRunInfo(double refreshHz, const wchar_t* pacerMode, bool compositionEnabled, int radiusDip, bool benchmark)
{
    refreshHz_ = refreshHz;
    pacerMode_ = pacerMode ? pacerMode : L"unknown";
    compositionEnabled_ = compositionEnabled;
    radiusDip_ = radiusDip;
    benchmark_ = benchmark;
}

bool FrameStats::WriteSummary(const std::wstring& path) const
{
    if (samples_.empty())
        return false;
    if (!EnsureParentDirectoryExists(path))
        return false;

    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file)
        return false;

    auto collect = [this](double Sample::*member, bool positiveOnly) {
        std::vector<double> values;
        values.reserve(samples_.size());
        for (const auto& sample : samples_)
        {
            const double value = sample.*member;
            if (!positiveOnly || value > 0.0)
                values.push_back(value);
        }
        return values;
    };

    std::vector<double> intervals = collect(&Sample::intervalMs, true);
    std::vector<double> render = collect(&Sample::renderMs, false);
    std::vector<double> present = collect(&Sample::presentMs, false);
    std::vector<double> pacer = collect(&Sample::pacerWaitMs, false);
    std::sort(intervals.begin(), intervals.end());
    std::sort(render.begin(), render.end());
    std::sort(present.begin(), present.end());
    std::sort(pacer.begin(), pacer.end());

    double totalInterval = std::accumulate(intervals.begin(), intervals.end(), 0.0);
    double avgInterval = intervals.empty() ? 0.0 : totalInterval / static_cast<double>(intervals.size());
    double measuredFps = avgInterval > 0.0 ? 1000.0 / avgInterval : 0.0;
    const double targetMs = refreshHz_ > 1.0 ? 1000.0 / refreshHz_ : 0.0;
    std::size_t lateFrames = 0;
    if (targetMs > 0.0)
    {
        for (double interval : intervals)
        {
            if (interval > targetMs * 1.5)
                ++lateFrames;
        }
    }

    SYSTEMTIME local = {};
    GetLocalTime(&local);

    std::fprintf(file, "CursorHalo P0 frame stats\n");
    std::fprintf(file, "generated=%04u-%02u-%02u %02u:%02u:%02u\n",
        local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
    std::fprintf(file, "presents=%zu\n", samples_.size());
    std::fprintf(file, "measured_fps=%.3f\n", measuredFps);
    std::fprintf(file, "refresh_hz=%.3f\n", refreshHz_);
    std::fprintf(file, "target_interval_ms=%.3f\n", targetMs);
    std::fprintf(file, "pacer=%s\n", NarrowAscii(pacerMode_).c_str());
    std::fprintf(file, "composition_enabled=%d\n", compositionEnabled_ ? 1 : 0);
    std::fprintf(file, "radius_dip=%d\n", radiusDip_);
    std::fprintf(file, "benchmark=%d\n", benchmark_ ? 1 : 0);
    std::fprintf(file, "interval_ms_min=%.3f\n", intervals.empty() ? 0.0 : intervals.front());
    std::fprintf(file, "interval_ms_p50=%.3f\n", Percentile(intervals, 50.0));
    std::fprintf(file, "interval_ms_p95=%.3f\n", Percentile(intervals, 95.0));
    std::fprintf(file, "interval_ms_p99=%.3f\n", Percentile(intervals, 99.0));
    std::fprintf(file, "interval_ms_max=%.3f\n", intervals.empty() ? 0.0 : intervals.back());
    std::fprintf(file, "late_frames_over_1.5x=%zu\n", lateFrames);
    std::fprintf(file, "render_ms_p50=%.3f\n", Percentile(render, 50.0));
    std::fprintf(file, "render_ms_p99=%.3f\n", Percentile(render, 99.0));
    std::fprintf(file, "present_ms_p50=%.3f\n", Percentile(present, 50.0));
    std::fprintf(file, "present_ms_p99=%.3f\n", Percentile(present, 99.0));
    std::fprintf(file, "dwm_wait_ms_p50=%.3f\n", Percentile(pacer, 50.0));
    std::fprintf(file, "dwm_wait_ms_p99=%.3f\n", Percentile(pacer, 99.0));
    std::fclose(file);
    return true;
}
