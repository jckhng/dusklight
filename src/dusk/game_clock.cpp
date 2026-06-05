#include "dusk/game_clock.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <dusk/frame_interpolation.h>
#include <dusk/logging.h>

namespace dusk::game_clock {

using clock = std::chrono::steady_clock;

bool s_initialized = false;
clock::time_point s_previous_sample{};
clock::time_point s_current_snapshot_time{};
bool s_portmaster_safe_pacing_allowed = true;
const char* s_portmaster_safe_pacing_block_reason = "startup";

std::unordered_map<uintptr_t, clock::time_point> s_interval_last_sample;

constexpr clock::duration kSimPeriodDuration =
    std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(sim_pace()));
constexpr clock::duration kAbnormalGapResetThreshold = std::chrono::milliseconds(250);
constexpr int kMaxSimTicksPerFrame = 2;

int parse_portmaster_int_env(const char* name, int fallback, int min_value, int max_value) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value) {
        DuskLog.warn("Ignoring invalid {}={}", name, value);
        return fallback;
    }

    if (parsed < min_value) {
        parsed = min_value;
    } else if (parsed > max_value) {
        parsed = max_value;
    }
    return static_cast<int>(parsed);
}

int portmaster_safe_pacing_fps() {
    static const int fps = [] {
        const int parsed = parse_portmaster_int_env("DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS", 0, 0, 30);
        if (parsed > 0) {
            DuskLog.info("PortMaster safe pacing enabled: target_fps={}", parsed);
        }
        return parsed;
    }();
    return fps;
}

int portmaster_max_sim_ticks_per_frame() {
    static const int ticks = [] {
        const int parsed = parse_portmaster_int_env("DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS", kMaxSimTicksPerFrame, 1, 5);
        if (portmaster_safe_pacing_fps() > 0) {
            DuskLog.info("PortMaster safe pacing max sim ticks per render={}", parsed);
        }
        return parsed;
    }();
    return ticks;
}

void ensure_initialized() {
    if (s_initialized) {
        return;
    }
    s_previous_sample = clock::now();
    s_current_snapshot_time = s_previous_sample;
    s_initialized = true;
}

void reset_frame_timer() {
    ensure_initialized();
    s_previous_sample = clock::now();
    s_current_snapshot_time = s_previous_sample - kSimPeriodDuration;
}

void set_portmaster_safe_pacing_allowed(bool allowed, const char* reason) {
    if (s_portmaster_safe_pacing_allowed == allowed) {
        s_portmaster_safe_pacing_block_reason = reason != nullptr ? reason : "unknown";
        return;
    }

    s_portmaster_safe_pacing_allowed = allowed;
    s_portmaster_safe_pacing_block_reason = reason != nullptr ? reason : "unknown";

    if (portmaster_safe_pacing_fps() > 0) {
        reset_frame_timer();
        if (allowed) {
            DuskLog.info("PortMaster safe pacing guard: allowed");
        } else {
            DuskLog.info("PortMaster safe pacing guard: suppressed ({})", s_portmaster_safe_pacing_block_reason);
        }
    }
}

MainLoopPacer advance_main_loop() {
    ensure_initialized();

    const clock::time_point now = clock::now();
    const clock::duration frame_gap = now - s_previous_sample;
    const float presentation_dt = std::chrono::duration<float>(frame_gap).count();
    s_previous_sample = now;

    MainLoopPacer out{};
    out.presentation_dt_seconds = presentation_dt;

    const bool should_interpolate = effective_frame_interp_mode() != dusk::FrameInterpMode::Off &&
                                    !dusk::getTransientSettings().skipFrameRateLimit;
    out.is_interpolating = should_interpolate;
    out.sim_pace = sim_pace();

    if (!should_interpolate) {
        s_current_snapshot_time = now;
        out.sim_ticks_to_run = 1;
        return out;
    }

    if (frame_gap > kAbnormalGapResetThreshold) {
        s_current_snapshot_time = now - kSimPeriodDuration;
        out.sim_ticks_to_run = 0;
        return out;
    }

    int sim_ticks_to_run = 0;
    clock::time_point projected_snapshot_time = s_current_snapshot_time;
    const clock::time_point render_time = now - kSimPeriodDuration;
    const int max_sim_ticks = portmaster_safe_pacing_fps() > 0 ? portmaster_max_sim_ticks_per_frame() : kMaxSimTicksPerFrame;
    while (sim_ticks_to_run < max_sim_ticks && projected_snapshot_time < render_time) {
        projected_snapshot_time += kSimPeriodDuration;
        sim_ticks_to_run++;
    }
    out.sim_ticks_to_run = sim_ticks_to_run;
    return out;
}

void commit_sim_tick() {
    ensure_initialized();
    s_current_snapshot_time += kSimPeriodDuration;
}

float sample_interpolation_step() {
    ensure_initialized();
    const float step =
        std::chrono::duration<float>(clock::now() - s_current_snapshot_time).count() / sim_pace();
    return std::clamp(step, 0.0f, 1.0f);
}

FrameInterpMode effective_frame_interp_mode() {
    if (portmaster_safe_pacing_fps() > 0) {
        if (!s_portmaster_safe_pacing_allowed) {
            return FrameInterpMode::Off;
        }
        return FrameInterpMode::Capped;
    }
    return dusk::getSettings().game.enableFrameInterpolation.getValue();
}

double effective_max_frame_rate() {
    const int safe_pacing_fps = portmaster_safe_pacing_fps();
    if (safe_pacing_fps > 0) {
        return static_cast<double>(safe_pacing_fps);
    }
    return dusk::getSettings().video.maxFrameRate.getValue();
}

float consume_interval(const void* consumer) {
    ensure_initialized();
    const uintptr_t key = reinterpret_cast<uintptr_t>(consumer);
    const clock::time_point now = clock::now();

    float dt = ui_initial_dt();
    const auto it = s_interval_last_sample.find(key);
    if (it != s_interval_last_sample.end()) {
        dt = std::chrono::duration<float>(now - it->second).count();
        dt = std::min(dt, ui_maximum_dt());
    }
    s_interval_last_sample[key] = now;
    return dt;
}

}  // namespace dusk::game_clock
