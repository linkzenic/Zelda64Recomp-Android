#include <cmath>
#include <cstdlib>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "zelda_config.h"
#include "recomp_input.h"
#include "recomp_ui.h"
#include "zelda_render.h"
#include "zelda_sound.h"
#include "zelda_clock_overlay.h"
#include "zelda_save_editor.h"
#include "librecomp/helpers.hpp"
#include "../patches/input.h"
#include "../patches/graphics.h"
#include "../patches/sound.h"
#include "../patches/touch_hud.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"

static bool android_n64_mode_enabled() {
#if defined(__ANDROID__)
    const char* n64_mode = std::getenv("APP_N64_MODE");
    const char* safe_mode = std::getenv("APP_SAFE_MODE");
    return (n64_mode != nullptr && n64_mode[0] == '1') ||
        (safe_mode != nullptr && safe_mode[0] == '1');
#else
    return false;
#endif
}

extern "C" void recomp_update_inputs(uint8_t* rdram, recomp_context* ctx) {
    recomp::poll_inputs();
}

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

extern "C" void recomp_get_gyro_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recomp::get_gyro_deltas(x_out, y_out);
}

extern "C" void recomp_get_mouse_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recomp::get_mouse_deltas(x_out, y_out);
}

extern "C" void recomp_get_clock_style(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, zelda64::get_clock_style() == zelda64::ClockStyle::Original ? 0 : 1);
}

extern "C" void recomp_get_swap_minimap_and_rupees(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, zelda64::get_swap_minimap_and_rupees());
}

extern "C" void recomp_get_dpad_items_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, !android_n64_mode_enabled() && zelda64::get_dpad_items_enabled());
}

extern "C" void recomp_get_dpad_items_mode_for_dpad_builtin(uint8_t* rdram, recomp_context* ctx) {
    const auto mode = android_n64_mode_enabled()
        ? zelda64::DpadItemsMode::Off
        : zelda64::get_dpad_items_mode();
    _return<s32>(ctx, static_cast<s32>(mode));
}

extern "C" void recomp_set_3ds_clock_state(uint8_t* rdram, recomp_context* ctx) {
    zelda64::set_clock_overlay_state({
        .visible = _arg<0, s32>(rdram, ctx) != 0,
        .alpha = _arg<1, s32>(rdram, ctx),
        .day = _arg<2, s32>(rdram, ctx),
        .current_time_seconds = _arg<3, s32>(rdram, ctx),
        .time_until_crash_seconds = static_cast<s32>(MEM_W(0x10, ctx->r29)),
        .time_speed_offset = static_cast<s32>(MEM_W(0x14, ctx->r29)),
        .final_hours = static_cast<s32>(MEM_W(0x18, ctx->r29)) != 0,
    });
}

extern "C" void recomp_get_clock_texture_pack_loaded(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, zelda64::get_clock_style() == zelda64::ClockStyle::Import &&
                          zelda64::get_clock_texture_pack_loaded());
}

extern "C" void recomp_save_editor_set_snapshot_value(uint8_t* rdram, recomp_context* ctx) {
    auto id = static_cast<zelda64::save_editor::ValueId>(_arg<0, s32>(rdram, ctx));
    zelda64::save_editor::set_snapshot_value(id, _arg<1, s32>(rdram, ctx));
}

extern "C" void recomp_save_editor_get_pending_value(uint8_t* rdram, recomp_context* ctx) {
    auto id = static_cast<zelda64::save_editor::ValueId>(_arg<0, s32>(rdram, ctx));
    _return(ctx, zelda64::save_editor::get_pending_value(id));
}

extern "C" void recomp_save_editor_should_apply_pending(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, zelda64::save_editor::should_apply_pending() ? 1 : 0);
}

extern "C" void recomp_save_editor_clear_pending(uint8_t* rdram, recomp_context* ctx) {
    zelda64::save_editor::clear_pending();
}

extern "C" void recomp_should_use_3ds_clock_overlay(uint8_t* rdram, recomp_context* ctx) {
    // Imported texture packs use the RmlUi overlay. The bundled 3DS clock is
    // rendered directly by the native game patch.
    _return<s32>(ctx, zelda64::get_clock_style() == zelda64::ClockStyle::Import);
}

extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx) {
    float a = _arg<0, float>(rdram, ctx);
    float b = ctx->f14.fl; //_arg<1, float>(rdram, ctx);

    _return(ctx, std::pow(a, b));
}

extern "C" void recomp_get_target_framerate(uint8_t* rdram, recomp_context* ctx) {
    int frame_divisor = _arg<0, u32>(rdram, ctx);

    _return(ctx, ultramodern::get_target_framerate(60 / frame_divisor));
}

extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx) {
    int width, height;
    recompui::get_window_size(width, height);

    gpr width_out = _arg<0, PTR(u32)>(rdram, ctx);
    gpr height_out = _arg<1, PTR(u32)>(rdram, ctx);

    MEM_W(0, width_out) = (u32)width;
    MEM_W(0, height_out) = (u32)height;
}

extern "C" bool zelda64_android_get_touch_hud_anchor(
    int slot, float* x, float* y, float* size);
extern "C" void zelda64_android_set_touch_hud_content(uint32_t content);

extern "C" void recomp_get_touch_hud_rect(uint8_t* rdram, recomp_context* ctx) {
#if defined(__ANDROID__)
    const int slot = _arg<0, s32>(rdram, ctx);
    const bool rightAligned = _arg<1, s32>(rdram, ctx) != 0;
    float x;
    float y;
    float size;
    if (!zelda64_android_get_touch_hud_anchor(slot, &x, &y, &size)) {
        _return<s32>(ctx, 0);
        return;
    }

    int windowWidth;
    int windowHeight;
    recompui::get_window_size(windowWidth, windowHeight);
    float aspect = windowHeight > 0
        ? static_cast<float>(windowWidth) / static_cast<float>(windowHeight)
        : 4.0f / 3.0f;
    if (ultramodern::renderer::get_graphics_config().ar_option ==
        ultramodern::renderer::AspectRatio::Original) {
        aspect = 4.0f / 3.0f;
    } else {
        aspect = std::max(aspect, 4.0f / 3.0f);
    }
    const float virtualWidth = 240.0f * aspect;
    const int projectedSize = std::clamp(
        static_cast<int>(std::lround(size * 240.0f)), 8, 120);
    const int projectedX = rightAligned
        ? static_cast<int>(std::lround(320.0f - (1.0f - x) * virtualWidth - projectedSize * .5f))
        : static_cast<int>(std::lround(x * virtualWidth - projectedSize * .5f));
    const int projectedY = static_cast<int>(std::lround(y * 240.0f - projectedSize * .5f));

    const gpr rectOut = _arg<2, PTR(TouchHudRect)>(rdram, ctx);
    MEM_W(offsetof(TouchHudRect, x), rectOut) = projectedX;
    MEM_W(offsetof(TouchHudRect, y), rectOut) = projectedY;
    MEM_W(offsetof(TouchHudRect, size), rectOut) = projectedSize;
    _return<s32>(ctx, 1);
#else
    _return<s32>(ctx, 0);
#endif
}

extern "C" void recomp_set_touch_hud_content(uint8_t* rdram, recomp_context* ctx) {
#if defined(__ANDROID__)
    zelda64_android_set_touch_hud_content(_arg<0, u32>(rdram, ctx));
#endif
}

extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);

    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return(ctx, original);
            return;
        case ultramodern::renderer::AspectRatio::Expand:
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
    }
}

extern "C" void recomp_get_targeting_mode(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<int>(zelda64::get_targeting_mode()));
}

extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, zelda64::get_bgm_volume() / 100.0f);
}

extern "C" void recomp_get_low_health_beeps_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(zelda64::get_low_health_beeps_enabled()));
}

extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(ultramodern::time_since_start()).count()));
}

extern "C" void recomp_get_autosave_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<s32>(zelda64::get_autosave_mode() == zelda64::AutosaveMode::On));
}

extern "C" void recomp_load_overlays(uint8_t * rdram, recomp_context * ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom, ram, size);
}

extern "C" void recomp_high_precision_fb_enabled(uint8_t * rdram, recomp_context * ctx) {
    _return(ctx, static_cast<s32>(zelda64::renderer::RT64HighPrecisionFBEnabled()));
}

extern "C" void recomp_get_resolution_scale(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, ultramodern::get_resolution_scale());
}

extern "C" void recomp_get_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    zelda64::CameraInvertMode mode = zelda64::get_camera_invert_mode();

    *x_out = (mode == zelda64::CameraInvertMode::InvertX || mode == zelda64::CameraInvertMode::InvertBoth);
    *y_out = (mode == zelda64::CameraInvertMode::InvertY || mode == zelda64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_analog_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    zelda64::CameraInvertMode mode = zelda64::get_analog_camera_invert_mode();

    *x_out = (mode == zelda64::CameraInvertMode::InvertX || mode == zelda64::CameraInvertMode::InvertBoth);
    *y_out = (mode == zelda64::CameraInvertMode::InvertY || mode == zelda64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_analog_cam_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, zelda64::get_analog_cam_mode() == zelda64::AnalogCamMode::On);
}

extern "C" void recomp_get_camera_inputs(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    // TODO expose this in the menu
    constexpr float radial_deadzone = 0.05f;

    float x, y;

    recomp::get_right_analog(&x, &y);

    float magnitude = sqrtf(x * x + y * y);

    if (magnitude < radial_deadzone) {
        *x_out = 0.0f;
        *y_out = 0.0f;
    }
    else {
        float x_normalized = x / magnitude;
        float y_normalized = y / magnitude;

        *x_out = x_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
        *y_out = y_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
    }
}

extern "C" void recomp_set_right_analog_suppressed(uint8_t* rdram, recomp_context* ctx) {
    s32 suppressed = _arg<0, s32>(rdram, ctx);

    recomp::set_right_analog_suppressed(suppressed);
}
