#pragma once

#include <cstdint>

namespace zelda64 {

struct ClockOverlayState {
    bool visible = false;
    int alpha = 0;
    int day = 1;
    int current_time_seconds = 0;
    int time_until_crash_seconds = 0;
    int time_speed_offset = 0;
    bool final_hours = false;
};

struct PauseSavePromptOverlayState {
    bool visible = false;
    int alpha = 0;
    int prompt_choice = 0;
    int save_prompt_state = 0;
};

void set_clock_overlay_state(const ClockOverlayState& state);
ClockOverlayState get_clock_overlay_state();
void set_pause_save_prompt_overlay_state(const PauseSavePromptOverlayState& state);
PauseSavePromptOverlayState get_pause_save_prompt_overlay_state();
void set_clock_texture_pack_loaded(bool loaded);
bool get_clock_texture_pack_loaded();

}
