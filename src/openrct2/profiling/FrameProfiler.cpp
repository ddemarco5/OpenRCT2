/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "FrameProfiler.hpp"

#include <cinttypes>
#include <cstdio>
#include <vector>

namespace OpenRCT2::Profiling
{
    // Global state for accumulated statistics
    struct AccumulatedStats
    {
        std::array<std::chrono::nanoseconds, static_cast<size_t>(FramePhase::Count)> totalTimes{};
        uint32_t frameCount = 0;
    };

    static FrameMetrics g_currentFrameMetrics;
    static AccumulatedStats g_accumulatedStats;           // For periodic summaries
    static AccumulatedStats g_sessionStats;               // For final session summary
    static bool g_periodicPrintingEnabled = false;        // Default: disabled
    static uint32_t g_periodicFrameCounter = 0;

    void FrameMetrics::Reset()
    {
        phaseTimes.fill(std::chrono::nanoseconds{});
    }

    void FrameMetrics::Record(FramePhase phase, std::chrono::nanoseconds duration)
    {
        phaseTimes[static_cast<size_t>(phase)] += duration;
    }

    FramePhaseTimer::FramePhaseTimer(FramePhase phase)
        : _phase(phase)
        , _start(std::chrono::high_resolution_clock::now())
        , _active(true)
    {
    }

    FramePhaseTimer::~FramePhaseTimer()
    {
        if (_active)
        {
            Stop();
        }
    }

    void FramePhaseTimer::Stop()
    {
        if (!_active)
            return;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - _start);
        GetCurrentFrameMetrics().Record(_phase, duration);
        _active = false;
    }

    FrameMetrics& GetCurrentFrameMetrics()
    {
        return g_currentFrameMetrics;
    }

    void BeginFrame()
    {
        g_currentFrameMetrics.Reset();
        g_currentFrameMetrics.frameNumber++;
    }

    void EndFrame()
    {
        // Accumulate stats for periodic reporting
        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            g_accumulatedStats.totalTimes[i] += g_currentFrameMetrics.phaseTimes[i];
        }
        g_accumulatedStats.frameCount++;

        // Accumulate stats for session summary (never reset)
        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            g_sessionStats.totalTimes[i] += g_currentFrameMetrics.phaseTimes[i];
        }
        g_sessionStats.frameCount++;

        // Periodic printing (if enabled)
        if (g_periodicPrintingEnabled)
        {
            if (++g_periodicFrameCounter >= 60)
            {
                PrintFrameSummary(60);
                g_periodicFrameCounter = 0;
            }
        }
    }

    void PrintFrameSummary(uint32_t numFrames)
    {
        if (g_accumulatedStats.frameCount == 0)
            return;

        uint32_t framesToReport = std::min(numFrames, g_accumulatedStats.frameCount);
        
        // Calculate averages
        auto totalTime = std::chrono::nanoseconds{};
        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            totalTime += g_accumulatedStats.totalTimes[i];
        }
        
        // Convert to milliseconds for display
        auto msTotal = std::chrono::duration_cast<std::chrono::microseconds>(totalTime).count() / 1000.0 / framesToReport;
        
        std::printf("[Profiler] Frame summary (avg over %u frames, total %.2f ms, ~%.1f fps):\n", 
                  framesToReport, msTotal, 1000.0 / msTotal);
        
        const char* phaseNames[] = {
            "PaintSessionGenerate",
            "PaintSessionArrange", 
            "PaintDrawStructs",
            "CopyBitsToTexture",
            "SDLRenderPresent"
        };
        
        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            auto avgMs = std::chrono::duration_cast<std::chrono::microseconds>(g_accumulatedStats.totalTimes[i]).count() / 1000.0 / framesToReport;
            double pct = (totalTime.count() > 0) ? (100.0 * g_accumulatedStats.totalTimes[i].count() / totalTime.count()) : 0.0;
            std::printf("  %-22s: %6.2f ms (%5.1f%%)\n", phaseNames[i], avgMs, pct);
        }
        std::printf("  ------------------------------\n");

        // Reset for next batch
        ResetAccumulatedStats();
    }

    void ResetAccumulatedStats()
    {
        g_accumulatedStats.totalTimes.fill(std::chrono::nanoseconds{});
        g_accumulatedStats.frameCount = 0;
    }

    void PrintSessionSummary()
    {
        if (g_sessionStats.frameCount == 0)
        {
            std::printf("[Profiler] No frames recorded for session summary.\n");
            return;
        }

        uint32_t totalFrames = g_sessionStats.frameCount;

        // Calculate averages
        auto totalTime = std::chrono::nanoseconds{};
        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            totalTime += g_sessionStats.totalTimes[i];
        }

        // Convert to milliseconds for display
        auto msTotal = std::chrono::duration_cast<std::chrono::microseconds>(totalTime).count() / 1000.0 / totalFrames;
        double totalRuntimeSec = std::chrono::duration_cast<std::chrono::seconds>(totalTime).count();

        std::printf("\n");
        std::printf("[Profiler] ==================================================\n");
        std::printf("[Profiler] SESSION SUMMARY (%u frames, %.1f sec runtime)\n", totalFrames, totalRuntimeSec);
        std::printf("[Profiler] ==================================================\n");
        std::printf("[Profiler] Average frame time: %.2f ms (%.1f fps)\n", msTotal, 1000.0 / msTotal);
        std::printf("[Profiler] --------------------------------------------------\n");

        const char* phaseNames[] = {
            "PaintSessionGenerate",
            "PaintSessionArrange",
            "PaintDrawStructs",
            "CopyBitsToTexture",
            "SDLRenderPresent"
        };

        for (size_t i = 0; i < static_cast<size_t>(FramePhase::Count); ++i)
        {
            auto avgMs = std::chrono::duration_cast<std::chrono::microseconds>(g_sessionStats.totalTimes[i]).count() / 1000.0 / totalFrames;
            double pct = (totalTime.count() > 0) ? (100.0 * g_sessionStats.totalTimes[i].count() / totalTime.count()) : 0.0;
            std::printf("[Profiler]  %-22s: %6.2f ms (%5.1f%%)\n", phaseNames[i], avgMs, pct);
        }
        std::printf("[Profiler] ==================================================\n");

        std::printf("\n");
    }

    void SetPeriodicPrintingEnabled(bool enabled)
    {
        g_periodicPrintingEnabled = enabled;
        if (!enabled)
        {
            g_periodicFrameCounter = 0;
        }
    }

    bool IsPeriodicPrintingEnabled()
    {
        return g_periodicPrintingEnabled;
    }

} // namespace OpenRCT2::Profiling
