#pragma once

#include <cstdint>

namespace zelda64::android {

bool attach_touch_controller();
uint16_t consume_touch_game_buttons();

}
