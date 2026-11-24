#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <cstdint>

#include "rt64_probe.h"
#include "runtime_probe.h"

namespace {

constexpr char LogTag[] = "ZeldaAndroidProbe";

struct ProbeState {
    bool resumed = false;
    bool focused = false;
    bool window_ready = false;
    std::uint64_t command_count = 0;
    Rt64Probe rt64;
};

const char* command_name(std::int32_t command) {
    switch (command) {
    case APP_CMD_INPUT_CHANGED: return "INPUT_CHANGED";
    case APP_CMD_INIT_WINDOW: return "INIT_WINDOW";
    case APP_CMD_TERM_WINDOW: return "TERM_WINDOW";
    case APP_CMD_WINDOW_RESIZED: return "WINDOW_RESIZED";
    case APP_CMD_WINDOW_REDRAW_NEEDED: return "WINDOW_REDRAW_NEEDED";
    case APP_CMD_CONTENT_RECT_CHANGED: return "CONTENT_RECT_CHANGED";
    case APP_CMD_GAINED_FOCUS: return "GAINED_FOCUS";
    case APP_CMD_LOST_FOCUS: return "LOST_FOCUS";
    case APP_CMD_CONFIG_CHANGED: return "CONFIG_CHANGED";
    case APP_CMD_LOW_MEMORY: return "LOW_MEMORY";
    case APP_CMD_START: return "START";
    case APP_CMD_RESUME: return "RESUME";
    case APP_CMD_SAVE_STATE: return "SAVE_STATE";
    case APP_CMD_PAUSE: return "PAUSE";
    case APP_CMD_STOP: return "STOP";
    case APP_CMD_DESTROY: return "DESTROY";
    default: return "UNKNOWN";
    }
}

void handle_command(android_app* app, std::int32_t command) {
    auto& state = *static_cast<ProbeState*>(app->userData);
    ++state.command_count;

    __android_log_print(
        ANDROID_LOG_INFO,
        LogTag,
        "command=%s(%d) count=%llu resumed=%d focused=%d window=%p",
        command_name(command),
        command,
        static_cast<unsigned long long>(state.command_count),
        state.resumed ? 1 : 0,
        state.focused ? 1 : 0,
        static_cast<void*>(app->window));

    switch (command) {
    case APP_CMD_INIT_WINDOW:
        state.window_ready = app->window != nullptr;
        if (state.window_ready) {
            __android_log_print(
                ANDROID_LOG_INFO,
                LogTag,
                "window ready width=%d height=%d format=%d",
                ANativeWindow_getWidth(app->window),
                ANativeWindow_getHeight(app->window),
                ANativeWindow_getFormat(app->window));
            state.rt64.initialize(app->window);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        state.rt64.shutdown();
        state.window_ready = false;
        break;
    case APP_CMD_GAINED_FOCUS:
        state.focused = true;
        break;
    case APP_CMD_LOST_FOCUS:
        state.focused = false;
        break;
    case APP_CMD_RESUME:
        state.resumed = true;
        break;
    case APP_CMD_PAUSE:
        state.resumed = false;
        break;
    default:
        break;
    }
}

} // namespace

extern "C" void android_main(android_app* app) {
    ProbeState state{};
    app->userData = &state;
    app->onAppCmd = handle_command;

    __android_log_print(
        ANDROID_LOG_INFO,
        LogTag,
        "stage=0 native lifecycle probe started pointer_bits=%zu api=%d",
        sizeof(void*) * 8,
        app->activity->sdkVersion);

    RuntimeProbe::run();

    while (app->destroyRequested == 0) {
        android_poll_source* source = nullptr;
        int events = 0;
        const int timeout = state.resumed && state.window_ready ? 16 : -1;
        const int result = ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source));

        if (result == ALOOPER_POLL_ERROR) {
            __android_log_print(ANDROID_LOG_ERROR, LogTag, "ALooper_pollOnce failed");
            break;
        }
        if (source != nullptr) {
            source->process(app, source);
        }
    }

    state.rt64.shutdown();
    __android_log_print(ANDROID_LOG_INFO, LogTag, "stage=0 native lifecycle probe stopped");
}
