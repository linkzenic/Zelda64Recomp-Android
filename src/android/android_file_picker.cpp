#include "android_file_picker.h"

#include <SDL_system.h>
#include <jni.h>

#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "zelda_render.h"
#include "recomp_ui.h"

namespace {

std::mutex pickerMutex;
zelda64::android::FilePickerCallback pickerCallback;
zelda64::android::MultiFilePickerCallback multiPickerCallback;
std::optional<std::filesystem::path> selectedPath;
std::list<std::filesystem::path> selectedPaths;
bool pickerFinished = false;

bool beginPicker() {
    if (pickerCallback || multiPickerCallback) {
        return false;
    }
    selectedPath.reset();
    selectedPaths.clear();
    pickerFinished = false;
    return true;
}

bool launchPicker(const char* methodName) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return false;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID openMethod = activityClass == nullptr
        ? nullptr
        : env->GetMethodID(activityClass, methodName, "()V");
    if (openMethod != nullptr) {
        env->CallVoidMethod(activity, openMethod);
    }

    const bool launched = openMethod != nullptr && !env->ExceptionCheck();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (activityClass != nullptr) {
        env->DeleteLocalRef(activityClass);
    }
    env->DeleteLocalRef(activity);
    return launched;
}

}

void zelda64::android::open_file_picker(FilePickerCallback callback) {
    bool pickerAlreadyOpen = false;
    {
        const std::lock_guard lock(pickerMutex);
        if (!beginPicker()) {
            pickerAlreadyOpen = true;
        }
        else {
            pickerCallback = std::move(callback);
        }
    }

    if (pickerAlreadyOpen) {
        callback(false, {});
        return;
    }

    if (!launchPicker("openFilePicker")) {
        const std::lock_guard lock(pickerMutex);
        pickerFinished = true;
    }
}

void zelda64::android::open_mod_file_picker(MultiFilePickerCallback callback) {
    bool pickerAlreadyOpen = false;
    {
        const std::lock_guard lock(pickerMutex);
        if (!beginPicker()) {
            pickerAlreadyOpen = true;
        }
        else {
            multiPickerCallback = std::move(callback);
        }
    }

    if (pickerAlreadyOpen) {
        callback(false, {});
        return;
    }

    if (!launchPicker("openModFilePicker")) {
        const std::lock_guard lock(pickerMutex);
        pickerFinished = true;
    }
}

void zelda64::android::poll_file_picker() {
    FilePickerCallback callback;
    MultiFilePickerCallback multiCallback;
    std::optional<std::filesystem::path> path;
    std::list<std::filesystem::path> paths;
    {
        const std::lock_guard lock(pickerMutex);
        if ((!pickerCallback && !multiPickerCallback) || !pickerFinished) {
            return;
        }

        callback = std::move(pickerCallback);
        multiCallback = std::move(multiPickerCallback);
        path = std::move(selectedPath);
        paths = std::move(selectedPaths);
        pickerFinished = false;
    }

    if (callback) {
        callback(path.has_value(), path.value_or(std::filesystem::path{}));
    }
    else if (multiCallback) {
        multiCallback(!paths.empty(), paths);
    }

    if (path.has_value()) {
        std::error_code error;
        std::filesystem::remove(*path, error);
    }
    for (const std::filesystem::path& selected : paths) {
        std::error_code error;
        std::filesystem::remove(selected, error);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_nativeOnFilePickerResult(
    JNIEnv* env,
    jclass,
    jstring path) {
    std::optional<std::filesystem::path> result;
    if (path != nullptr) {
        const char* utfPath = env->GetStringUTFChars(path, nullptr);
        if (utfPath != nullptr) {
            result = std::filesystem::path(utfPath);
            env->ReleaseStringUTFChars(path, utfPath);
        }
    }

    const std::lock_guard lock(pickerMutex);
    selectedPath = std::move(result);
    pickerFinished = true;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_nativeOnModFilePickerResult(
    JNIEnv* env,
    jclass,
    jobjectArray paths) {
    std::list<std::filesystem::path> results;
    if (paths != nullptr) {
        const jsize count = env->GetArrayLength(paths);
        for (jsize index = 0; index < count; index++) {
            jstring path = static_cast<jstring>(env->GetObjectArrayElement(paths, index));
            if (path != nullptr) {
                const char* utfPath = env->GetStringUTFChars(path, nullptr);
                if (utfPath != nullptr) {
                    results.emplace_back(utfPath);
                    env->ReleaseStringUTFChars(path, utfPath);
                }
                env->DeleteLocalRef(path);
            }
        }
    }

    const std::lock_guard lock(pickerMutex);
    selectedPaths = std::move(results);
    pickerFinished = true;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_nativeReloadClockTexturePack(
    JNIEnv*,
    jclass) {
    recompui::request_clock_texture_reload();
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_zelda64recomp_cleanboot_CleanBootActivity_nativeSetRendererPaused(
    JNIEnv*,
    jclass,
    jboolean paused) {
    if (paused == JNI_TRUE) {
        zelda64::renderer::pause_for_background();
    }
    else {
        zelda64::renderer::resume_from_background();
    }
}
