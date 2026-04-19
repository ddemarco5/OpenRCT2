/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace OpenRCT2::Profiling
{
    enum class FramePhase : uint8_t
    {
        PaintSessionGenerate,
        PaintSessionArrange,
        PaintDrawStructs,
        CopyBitsToTexture,
        SDLRenderPresent,
        Count
    };

    struct FrameMetrics
    {
        std::array<std::chrono::nanoseconds, static_cast<size_t>(FramePhase::Count)> phaseTimes{};
        uint32_t frameNumber = 0;

        void Reset();
        void Record(FramePhase phase, std::chrono::nanoseconds duration);
    };

    // Simple RAII timer for frame phases
    class FramePhaseTimer
    {
        FramePhase _phase;
        std::chrono::high_resolution_clock::time_point _start;
        bool _active;

    public:
        explicit FramePhaseTimer(FramePhase phase);
        ~FramePhaseTimer();

        void Stop(); // Manual stop if needed
    };

    // Global frame metrics (simple global storage for single-threaded use)
    FrameMetrics& GetCurrentFrameMetrics();
    void BeginFrame();
    void EndFrame(); // Logs/accumulates the frame metrics

    // Console output for release builds - prints average over N frames
    void PrintFrameSummary(uint32_t numFrames = 60);

    // Print final session summary (call on game shutdown)
    void PrintSessionSummary();

    // Reset accumulated statistics
    void ResetAccumulatedStats();

    // Enable/disable periodic frame summary printing (default: disabled)
    void SetPeriodicPrintingEnabled(bool enabled);
    bool IsPeriodicPrintingEnabled();

} // namespace OpenRCT2::Profiling
