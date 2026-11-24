#include "android_touch_controller.h"

#include <android/log.h>
#include <jni.h>

#include <SDL.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>
#include <SDL_system.h>

#include <mutex>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace {

int virtualJoystickIndex = -1;
SDL_Joystick* virtualJoystick = nullptr;
SDL_JoystickID virtualJoystickInstanceId = -1;
std::mutex virtualJoystickMutex;
std::once_flag accelerometerConnectedLog;
std::once_flag gyroscopeConnectedLog;

constexpr int defaultTouchSensitivity = 100;

struct TouchHudAnchor {
    float x = 0;
    float y = 0;
    float size = 0;
};

std::array<TouchHudAnchor, 10> touchHudAnchors;
std::mutex touchHudMutex;
std::atomic<bool> touchHudActive{false};
std::atomic<uint32_t> touchHudContent{0};
std::atomic<uint16_t> touchGameButtonsHeld{0};
std::atomic<uint16_t> touchGameButtonsPressed{0};

void resetTouchGameButtons() {
    touchGameButtonsHeld = 0;
    touchGameButtonsPressed = 0;
}

jobject getActivity(JNIEnv* environment) {
    return static_cast<jobject>(SDL_AndroidGetActivity());
}

void clearJavaException(JNIEnv* environment) {
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
    }
}

int getTouchIntegerPreference(const char* methodName, int fallback) {
    JNIEnv* environment = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (environment == nullptr) {
        return fallback;
    }

    jobject activity = getActivity(environment);
    if (activity == nullptr) {
        return fallback;
    }

    jclass activityClass = environment->GetObjectClass(activity);
    jmethodID method = activityClass == nullptr
        ? nullptr
        : environment->GetMethodID(activityClass, methodName, "()I");
    int value = fallback;
    if (method != nullptr) {
        value = environment->CallIntMethod(activity, method);
    }
    clearJavaException(environment);
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    environment->DeleteLocalRef(activity);
    return value;
}

void setTouchIntegerPreference(const char* methodName, int value) {
    JNIEnv* environment = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (environment == nullptr) {
        return;
    }

    jobject activity = getActivity(environment);
    if (activity == nullptr) {
        return;
    }

    jclass activityClass = environment->GetObjectClass(activity);
    jmethodID method = activityClass == nullptr
        ? nullptr
        : environment->GetMethodID(activityClass, methodName, "(I)V");
    if (method != nullptr) {
        environment->CallVoidMethod(activity, method, static_cast<jint>(value));
    }
    clearJavaException(environment);
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    environment->DeleteLocalRef(activity);
}

void callTouchActivityMethod(const char* methodName) {
    JNIEnv* environment = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (environment == nullptr) {
        return;
    }
    jobject activity = getActivity(environment);
    if (activity == nullptr) {
        return;
    }
    jclass activityClass = environment->GetObjectClass(activity);
    jmethodID method = activityClass == nullptr
        ? nullptr
        : environment->GetMethodID(activityClass, methodName, "()V");
    if (method != nullptr) {
        environment->CallVoidMethod(activity, method);
    }
    clearJavaException(environment);
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    environment->DeleteLocalRef(activity);
}

}

extern "C" bool zelda64_android_toggle_settings_menu();
extern "C" bool zelda64_android_is_settings_menu_open();

bool zelda64::android::attach_touch_controller() {
    const std::lock_guard lock(virtualJoystickMutex);
    if (virtualJoystick != nullptr) {
        return true;
    }

    if ((SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) == 0) {
        return false;
    }

    virtualJoystickIndex = SDL_JoystickAttachVirtual(
        SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        6,
        16,
        0);
    if (virtualJoystickIndex < 0) {
        return false;
    }

    virtualJoystick = SDL_JoystickOpen(virtualJoystickIndex);
    if (virtualJoystick == nullptr) {
        SDL_JoystickDetachVirtual(virtualJoystickIndex);
        virtualJoystickIndex = -1;
        return false;
    }

    virtualJoystickInstanceId = SDL_JoystickInstanceID(virtualJoystick);

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_attachTouchController(
    JNIEnv*,
    jobject) {
    return zelda64::android::attach_touch_controller() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_detachTouchController(
    JNIEnv*,
    jobject) {
    resetTouchGameButtons();
    const std::lock_guard lock(virtualJoystickMutex);
    if (virtualJoystick != nullptr) {
        SDL_JoystickClose(virtualJoystick);
        virtualJoystick = nullptr;
    }
    if (virtualJoystickIndex >= 0) {
        SDL_JoystickDetachVirtual(virtualJoystickIndex);
        virtualJoystickIndex = -1;
    }
    virtualJoystickInstanceId = -1;
}

uint16_t zelda64::android::consume_touch_game_buttons() {
    const uint16_t pressed = touchGameButtonsPressed.exchange(0);
    return pressed | touchGameButtonsHeld.load();
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_setTouchGameButton(
    JNIEnv*, jobject, jint mask, jboolean down) {
    if (down == JNI_TRUE) {
        touchGameButtonsHeld.fetch_or(mask);
        touchGameButtonsPressed.fetch_or(mask);
    } else {
        touchGameButtonsHeld.fetch_and(static_cast<uint16_t>(~mask));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_resetTouchGameButtons(
    JNIEnv*, jobject) {
    resetTouchGameButtons();
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_setTouchButton(
    JNIEnv*,
    jobject,
    jint button,
    jboolean pressed) {
    const std::lock_guard lock(virtualJoystickMutex);
    if (virtualJoystick != nullptr) {
        SDL_JoystickSetVirtualButton(virtualJoystick, button, pressed == JNI_TRUE ? 1 : 0);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_setTouchAxis(
    JNIEnv*,
    jobject,
    jint axis,
    jshort value) {
    const std::lock_guard lock(virtualJoystickMutex);
    if (virtualJoystick != nullptr) {
        SDL_JoystickSetVirtualAxis(virtualJoystick, axis, value);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_submitAndroidMotionSensor(
    JNIEnv*,
    jobject,
    jint sensorType,
    jfloat x,
    jfloat y,
    jfloat z,
    jlong timestampNanoseconds) {
    SDL_JoystickID instanceId;
    {
        const std::lock_guard lock(virtualJoystickMutex);
        instanceId = virtualJoystickInstanceId;
    }
    if (instanceId < 0) {
        return;
    }

    SDL_Event event{};
    event.type = SDL_CONTROLLERSENSORUPDATE;
    event.csensor.timestamp = static_cast<Uint32>(timestampNanoseconds / 1'000'000);
    event.csensor.which = instanceId;
    event.csensor.sensor = sensorType == 0 ? SDL_SENSOR_ACCEL : SDL_SENSOR_GYRO;
    event.csensor.data[0] = x;
    event.csensor.data[1] = y;
    event.csensor.data[2] = z;
    event.csensor.timestamp_us = static_cast<Uint64>(timestampNanoseconds / 1'000);
    if (SDL_PushEvent(&event) > 0) {
        std::once_flag& connectedLog = sensorType == 0
            ? accelerometerConnectedLog
            : gyroscopeConnectedLog;
        std::call_once(connectedLog, [sensorType]() {
            __android_log_print(
                ANDROID_LOG_INFO,
                "ZeldaCleanBoot",
                "Android %s stream connected to SDL",
                sensorType == 0 ? "accelerometer" : "gyroscope");
        });
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_toggleIntegratedTouchSettingsNative(
    JNIEnv*,
    jobject) {
    return zelda64_android_toggle_settings_menu() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_isIntegratedTouchSettingsOpenNative(
    JNIEnv*,
    jobject) {
    return zelda64_android_is_settings_menu_open() ? JNI_TRUE : JNI_FALSE;
}

extern "C" bool zelda64_android_is_ui_capturing_touch();

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_isIntegratedTouchUiCapturingNative(
    JNIEnv*,
    jobject) {
    return zelda64_android_is_ui_capturing_touch() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_getIntegratedTouchContentNative(
    JNIEnv*,
    jobject) {
    return static_cast<jint>(touchHudContent.load());
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_setIntegratedTouchLayoutNative(
    JNIEnv* environment,
    jobject,
    jfloatArray values,
    jboolean active) {
    if (values == nullptr || environment->GetArrayLength(values) != 30) {
        touchHudActive = false;
        return;
    }
    std::array<float, 30> data{};
    environment->GetFloatArrayRegion(values, 0, data.size(), data.data());
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
        touchHudActive = false;
        return;
    }
    const std::lock_guard lock(touchHudMutex);
    for (size_t index = 0; index < touchHudAnchors.size(); ++index) {
        touchHudAnchors[index] = {
            data[index * 3], data[index * 3 + 1], data[index * 3 + 2]
        };
    }
    touchHudActive = active == JNI_TRUE;
}

extern "C" bool zelda64_android_get_touch_hud_anchor(
    int slot, float* x, float* y, float* size) {
    if (!touchHudActive || slot < 0 || slot >= static_cast<int>(touchHudAnchors.size())) {
        return false;
    }
    const std::lock_guard lock(touchHudMutex);
    const TouchHudAnchor anchor = touchHudAnchors[slot];
    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y) ||
        !std::isfinite(anchor.size) || anchor.x < 0 || anchor.x > 1 ||
        anchor.y < 0 || anchor.y > 1 || anchor.size <= 0 || anchor.size > .5f) {
        return false;
    }
    *x = anchor.x;
    *y = anchor.y;
    *size = anchor.size;
    return true;
}

extern "C" void zelda64_android_set_touch_hud_content(uint32_t content) {
    touchHudContent = content;
}

extern "C" __attribute__((visibility("default"))) bool
zelda64_android_are_touch_controls_disabled() {
    JNIEnv* environment = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (environment == nullptr) {
        return false;
    }

    jobject activity = getActivity(environment);
    if (activity == nullptr) {
        return false;
    }

    jclass activityClass = environment->GetObjectClass(activity);
    jmethodID method = activityClass == nullptr
        ? nullptr
        : environment->GetMethodID(activityClass, "areTouchControlsDisabledFromNative", "()Z");
    bool disabled = false;
    if (method != nullptr) {
        disabled = environment->CallBooleanMethod(activity, method) == JNI_TRUE;
    }
    clearJavaException(environment);
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    environment->DeleteLocalRef(activity);
    return disabled;
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_touch_controls_disabled(bool disabled) {
    if (disabled) {
        resetTouchGameButtons();
        // Stop relocating the native HUD before the UI-thread visibility update.
        touchHudActive = false;
        touchHudContent = 0;
    }

    JNIEnv* environment = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (environment == nullptr) {
        return;
    }

    jobject activity = getActivity(environment);
    if (activity == nullptr) {
        return;
    }

    jclass activityClass = environment->GetObjectClass(activity);
    jmethodID method = activityClass == nullptr
        ? nullptr
        : environment->GetMethodID(activityClass, "setTouchControlsDisabledFromNative", "(Z)V");
    if (method != nullptr) {
        environment->CallVoidMethod(activity, method, disabled ? JNI_TRUE : JNI_FALSE);
    }
    clearJavaException(environment);
    if (activityClass != nullptr) {
        environment->DeleteLocalRef(activityClass);
    }
    environment->DeleteLocalRef(activity);
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_touch_face_button_layout() {
    return getTouchIntegerPreference("getTouchFaceButtonLayoutFromNative", 0);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_touch_face_button_layout(int layout) {
    setTouchIntegerPreference("setTouchFaceButtonLayoutFromNative", layout);
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_integrated_touch_style() {
    return getTouchIntegerPreference("getIntegratedTouchStyleFromNative", 3);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_integrated_touch_style(int style) {
    setTouchIntegerPreference("setIntegratedTouchStyleFromNative", style);
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_integrated_touch_scale() {
    return getTouchIntegerPreference("getIntegratedTouchScaleFromNative", 100);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_integrated_touch_scale(int percent) {
    setTouchIntegerPreference("setIntegratedTouchScaleFromNative", percent);
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_integrated_touch_opacity() {
    return getTouchIntegerPreference("getIntegratedTouchOpacityFromNative", 55);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_integrated_touch_opacity(int percent) {
    setTouchIntegerPreference("setIntegratedTouchOpacityFromNative", percent);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_edit_integrated_touch_layout() {
    callTouchActivityMethod("editIntegratedTouchFromNative");
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_touch_camera_x_sensitivity() {
    return getTouchIntegerPreference("getTouchCameraXSensitivityFromNative", defaultTouchSensitivity);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_touch_camera_x_sensitivity(int sensitivity) {
    setTouchIntegerPreference("setTouchCameraXSensitivityFromNative", sensitivity);
}

extern "C" __attribute__((visibility("default"))) int
zelda64_android_get_touch_camera_y_sensitivity() {
    return getTouchIntegerPreference("getTouchCameraYSensitivityFromNative", defaultTouchSensitivity);
}

extern "C" __attribute__((visibility("default"))) void
zelda64_android_set_touch_camera_y_sensitivity(int sensitivity) {
    setTouchIntegerPreference("setTouchCameraYSensitivityFromNative", sensitivity);
}
