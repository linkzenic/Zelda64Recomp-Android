#include "zelda_clock_overlay.h"

#include <mutex>

namespace {
    std::mutex clock_overlay_mutex;
    zelda64::ClockOverlayState clock_overlay_state;
    zelda64::PauseSavePromptOverlayState pause_save_prompt_overlay_state;
    bool clock_texture_pack_loaded = false;
}

void zelda64::set_clock_overlay_state(const ClockOverlayState& state) {
    std::lock_guard lock{ clock_overlay_mutex };
    clock_overlay_state = state;
}

zelda64::ClockOverlayState zelda64::get_clock_overlay_state() {
    std::lock_guard lock{ clock_overlay_mutex };
    return clock_overlay_state;
}

void zelda64::set_pause_save_prompt_overlay_state(const PauseSavePromptOverlayState& state) {
    std::lock_guard lock{ clock_overlay_mutex };
    pause_save_prompt_overlay_state = state;
}

zelda64::PauseSavePromptOverlayState zelda64::get_pause_save_prompt_overlay_state() {
    std::lock_guard lock{ clock_overlay_mutex };
    return pause_save_prompt_overlay_state;
}

void zelda64::set_clock_texture_pack_loaded(bool loaded) {
    std::lock_guard lock{ clock_overlay_mutex };
    clock_texture_pack_loaded = loaded;
}

bool zelda64::get_clock_texture_pack_loaded() {
    std::lock_guard lock{ clock_overlay_mutex };
    return clock_texture_pack_loaded;
}
