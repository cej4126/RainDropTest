#pragma once

#include "stdafx.h"
#include <thread>
#include <chrono>

class time_it
{
public:
    using clock = std::chrono::high_resolution_clock;
    using time_stamp = std::chrono::steady_clock::time_point;

    // Average frame time per second.
    constexpr float dt_avg() const { return _dt_avg * 1e-6f; }

    void begin()
    {
        _start = clock::now();
    }

    void end()
    {
        auto dt = clock::now() - _start;
        _us_avg += ((float)std::chrono::duration_cast<std::chrono::microseconds>(dt).count() - _us_avg) / (float)_counter;
        ++_counter;
        _dt_avg = _us_avg;

        if (std::chrono::duration_cast<std::chrono::seconds>(clock::now() - _seconds).count() >= 1)
        {
            OutputDebugStringA("Avg. frame (ms): ");
            OutputDebugStringA(std::to_string(_us_avg * 0.001f).c_str());
            OutputDebugStringA((" " + std::to_string(_counter)).c_str());
            OutputDebugStringA(" fps");
            OutputDebugStringA("\n");
            _us_avg = 0.f;
            _counter = 1;
            _seconds = clock::now();
        }
    }

private:
    float       _dt_avg{ 16.7f };
    float       _us_avg{ 0.f };
    int         _counter{ 1 };
    time_stamp  _start;
    time_stamp  _seconds{ clock::now() };
};

//// Helper class for animation and simulation timing.
//class StepTimer
//{
//public:
//    StepTimer() :
//        m_left_over_ticks(0),
//        m_frame_count(0),
//        m_frames_per_second(0),
//        m_frames_this_second(0),
//        m_qpc_second_counter(0),
//        m_is_fixed_time_step(false),
//        m_target_elapsed_ticks(TicksPerSecond / 60)
//    {
//        QueryPerformanceFrequency(&m_qpc_frequency);
//        QueryPerformanceCounter(&m_qpc_last_time);
//
//        // Initialize max dalta to 1/10 of a second
//        m_qpc_max_delta = m_qpc_frequency.QuadPart / 10;
//    }
//
//    // Get elapsed time since the previous Update call.
//    UINT64 GetElapsedTicks() const { return m_elapsed_ticks; }
//    double GetElapsedSeconds() const { return TicksToSeconds(m_elapsed_ticks); }
//
//    // Get total time since the start of the program.
//    UINT64 GetTotalTicks() const { return m_total_ticks; }
//    double GetTotalSeconds() const { return TicksToSeconds(m_total_ticks); }
//
//    // Get total number of updates since start of the program.
//    UINT32 GetFrameCount() const { return m_frame_count; }
//
//    // Get the current framerate.
//    UINT32 GetFramesPerSecond() const { return m_frames_per_second; }
//
//    // Set whether to use fixed or variable timestep mode.
//    void SetFixedTimeStep(bool isFixedTimestep) { m_is_fixed_time_step = isFixedTimestep; }
//
//    // Set how often to call Update when in fixed timestep mode.
//    void SetTargetElapsedTicks(UINT64 targetElapsed) { m_target_elapsed_ticks = targetElapsed; }
//    void SetTargetElapsedSeconds(double targetElapsed) { m_target_elapsed_ticks = SecondsToTicks(targetElapsed); }
//
//    // Integer format represents time using 10,000,000 ticks per second.
//    static const UINT64 TicksPerSecond = 10000000;
//
//    static double TicksToSeconds(UINT64 ticks) { return static_cast<double>(ticks) / TicksPerSecond; }
//    static UINT64 SecondsToTicks(double seconds) { return static_cast<UINT64>(seconds * TicksPerSecond); }
//
//    // After an intentional timing discontinuity (for instance a blocking IO operation)
//    // call this to avoid having the fixed timestep logic attempt a set of catch-up 
//    // Update calls.
//
//    void ResetElapsedTime()
//    {
//        QueryPerformanceCounter(&m_qpc_last_time);
//
//        m_left_over_ticks = 0;
//        m_frames_per_second = 0;
//        m_frames_this_second = 0;
//        m_qpc_second_counter = 0;
//    }
//
//	typedef void(*LPUPDATEFUNC) (void);
//
//	// Update timer state, calling the specified Update function the appropriate number of times.
//	void Tick(LPUPDATEFUNC update)
//	{
//		// Query the current time.
//		LARGE_INTEGER currentTime;
//
//		QueryPerformanceCounter(&currentTime);
//
//		UINT64 timeDelta = currentTime.QuadPart - m_qpc_last_time.QuadPart;
//
//		m_qpc_last_time = currentTime;
//		m_qpc_second_counter += timeDelta;
//
//		// Clamp excessively large time deltas (e.g. after paused in the debugger).
//		if (timeDelta > m_qpc_max_delta)
//		{
//			timeDelta = m_qpc_max_delta;
//		}
//
//		// Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
//		timeDelta *= TicksPerSecond;
//		timeDelta /= m_qpc_frequency.QuadPart;
//
//		UINT32 lastFrameCount = m_frame_count;
//
//		if (m_is_fixed_time_step)
//		{
//			// Fixed timestep update logic
//
//			// If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
//			// the clock to exactly match the target value. This prevents tiny and irrelevant errors
//			// from accumulating over time. Without this clamping, a game that requested a 60 fps
//			// fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
//			// accumulate enough tiny errors that it would drop a frame. It is better to just round 
//			// small deviations down to zero to leave things running smoothly.
//
//			if (abs(static_cast<int>(timeDelta - m_target_elapsed_ticks)) < TicksPerSecond / 4000)
//			{
//				timeDelta = m_target_elapsed_ticks;
//			}
//
//			m_left_over_ticks += timeDelta;
//
//			while (m_left_over_ticks >= m_target_elapsed_ticks)
//			{
//				m_elapsed_ticks = m_target_elapsed_ticks;
//				m_total_ticks += m_target_elapsed_ticks;
//				m_left_over_ticks -= m_target_elapsed_ticks;
//				m_frame_count++;
//
//				if (update)
//				{
//					update();
//				}
//			}
//		}
//		else
//		{
//			// Variable timestep update logic.
//			m_elapsed_ticks = timeDelta;
//			m_total_ticks += timeDelta;
//			m_left_over_ticks = 0;
//			m_frame_count++;
//
//			if (update)
//			{
//				update();
//			}
//		}
//
//		// Track the current framerate.
//		if (m_frame_count != lastFrameCount)
//		{
//			m_frames_this_second++;
//		}
//
//		if (m_qpc_second_counter >= static_cast<UINT64>(m_qpc_frequency.QuadPart))
//		{
//			m_frames_per_second = m_frames_this_second;
//			m_frames_this_second = 0;
//			m_qpc_second_counter %= m_qpc_frequency.QuadPart;
//		}
//	}
//
//private:
//    // Source timing data uses QPC units.
//    LARGE_INTEGER m_qpc_frequency;
//    LARGE_INTEGER m_qpc_last_time;
//    UINT64 m_qpc_max_delta;
//
//    // Derived timing data uses a canonical tick format.
//    UINT64 m_elapsed_ticks;
//    UINT64 m_total_ticks;
//    UINT64 m_left_over_ticks;
//
//    // Members for tracking the framerate.
//    UINT32 m_frame_count;
//    UINT32 m_frames_per_second;
//    UINT32 m_frames_this_second;
//    UINT64 m_qpc_second_counter;
//
//    // Members for configuring fixed timestep mode.
//    bool m_is_fixed_time_step;
//    UINT64 m_target_elapsed_ticks;
//};
