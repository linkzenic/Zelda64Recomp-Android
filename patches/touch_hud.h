#ifndef __PATCH_TOUCH_HUD_H__
#define __PATCH_TOUCH_HUD_H__

#include "patch_helpers.h"

typedef struct {
    s32 x;
    s32 y;
    s32 size;
} TouchHudRect;

DECLARE_FUNC(s32, recomp_get_touch_hud_rect, s32, s32, TouchHudRect*);
DECLARE_FUNC(void, recomp_set_touch_hud_content, u32);

#endif
