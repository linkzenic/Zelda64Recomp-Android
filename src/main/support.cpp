#include "zelda_support.h"
#include <SDL.h>
#include "nfd.h"
#include <cstdlib>
#include "RmlUi/Core.h"

#if defined(__ANDROID__)
#include "../android/android_file_picker.h"
#include <SDL_system.h>
#include <jni.h>
#endif

namespace {
#if defined(__ANDROID__)
bool call_android_activity_method(const char* method_name) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return false;
    }

    jclass activity_class = env->GetObjectClass(activity);
    jmethodID method = activity_class == nullptr
        ? nullptr
        : env->GetMethodID(activity_class, method_name, "()V");
    if (method != nullptr) {
        env->CallVoidMethod(activity, method);
    }

    const bool succeeded = method != nullptr && !env->ExceptionCheck();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (activity_class != nullptr) {
        env->DeleteLocalRef(activity_class);
    }
    env->DeleteLocalRef(activity);
    return succeeded;
}
#endif
}

namespace zelda64 {
    // MARK: - Internal Helpers
    void perform_file_dialog_operation(const std::function<void(bool, const std::filesystem::path&)>& callback) {
        nfdnchar_t* native_path = nullptr;
        nfdresult_t result = NFD_OpenDialogN(&native_path, nullptr, 0, nullptr);

        bool success = (result == NFD_OKAY);
        std::filesystem::path path;

        if (success) {
            path = std::filesystem::path{native_path};
            NFD_FreePathN(native_path);
        }

        callback(success, path);
    }

    void perform_file_dialog_operation_multiple(const std::function<void(bool, const std::list<std::filesystem::path>&)>& callback) {
        const nfdpathset_t* native_paths = nullptr;
        nfdresult_t result = NFD_OpenDialogMultipleN(&native_paths, nullptr, 0, nullptr);

        bool success = (result == NFD_OKAY);
        std::list<std::filesystem::path> paths;
        nfdpathsetsize_t count = 0;

        if (success) {
            NFD_PathSet_GetCount(native_paths, &count);
            for (nfdpathsetsize_t i = 0; i < count; i++) {
                nfdnchar_t* cur_path = nullptr;
                nfdresult_t cur_result = NFD_PathSet_GetPathN(native_paths, i, &cur_path);
                if (cur_result == NFD_OKAY) {
                    paths.emplace_back(std::filesystem::path{cur_path});
                }
            }
            NFD_PathSet_Free(native_paths);
        }

        callback(success, paths);
    }

    // MARK: - Public API

    std::filesystem::path get_program_path() {
#if defined(__ANDROID__)
        if (const char* program_path = std::getenv("APP_PROGRAM_PATH")) {
            return program_path;
        }
        return "";
#elif defined(__APPLE__)
        return get_bundle_resource_directory();
#elif defined(__linux__) && defined(RECOMP_FLATPAK)
        return "/app/bin";
#else
        return "";
#endif
    }

    std::filesystem::path get_asset_path(const char* asset) {
        return get_program_path() / "assets" / asset;
    }

    void open_file_dialog(std::function<void(bool success, const std::filesystem::path& path)> callback) {
#if defined(__ANDROID__)
        android::open_file_picker(std::move(callback));
#elif defined(__APPLE__)
        dispatch_on_ui_thread([callback]() {
            perform_file_dialog_operation(callback);
        });
#else
        perform_file_dialog_operation(callback);
#endif
    }

    void open_json_file_dialog(std::function<void(bool success, const std::filesystem::path& path)> callback) {
        open_file_dialog(std::move(callback));
    }

    void open_file_dialog_multiple(std::function<void(bool success, const std::list<std::filesystem::path>& paths)> callback) {
#if defined(__ANDROID__)
        android::open_mod_file_picker(std::move(callback));
#elif defined(__APPLE__)
        dispatch_on_ui_thread([callback]() {
            perform_file_dialog_operation_multiple(callback);
        });
#else
        perform_file_dialog_operation_multiple(callback);
#endif
    }

    void open_driver_file_dialog() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("openDriverFilePicker")) {
            show_error_message_box("GPU driver", "Unable to open the Android file picker.");
        }
#else
        show_error_message_box("GPU driver", "Custom GPU drivers are only available on Android.");
#endif
    }

    void clear_custom_driver() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("clearCustomDriver")) {
            show_error_message_box("GPU driver", "Unable to select the system GPU driver.");
        }
#else
        show_error_message_box("GPU driver", "Custom GPU drivers are only available on Android.");
#endif
    }

    void open_clock_texture_file_dialog() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("openClockTextureFilePicker")) {
            show_error_message_box("Clock texture pack", "Unable to open the Android file picker.");
        }
#else
        show_error_message_box("Clock texture pack", "Clock texture import is only available on Android.");
#endif
    }

    void open_legacy_data_inventory_picker() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("openLegacyDataInventoryPicker")) {
            show_error_message_box("Existing data", "Unable to open the Android folder picker.");
        }
#else
        show_error_message_box("Existing data", "Existing Android data can only be inspected on Android.");
#endif
    }

    void open_legacy_data_staging_picker() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("openLegacyDataStagingPicker")) {
            show_error_message_box("Existing data", "Unable to open the Android folder picker.");
        }
#else
        show_error_message_box("Existing data", "Existing Android data can only be staged on Android.");
#endif
    }

    void schedule_legacy_data_activation() {
#if defined(__ANDROID__)
        if (!call_android_activity_method("scheduleLegacyDataActivation")) {
            show_error_message_box("Existing data", "Unable to schedule staged-data activation.");
        }
#else
        show_error_message_box("Existing data", "Existing Android data can only be activated on Android.");
#endif
    }

    void show_error_message_box(const char *title, const char *message) {
#ifdef __APPLE__
    std::string title_copy(title);
    std::string message_copy(message);

    dispatch_on_ui_thread([title_copy, message_copy] {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title_copy.c_str(), message_copy.c_str(), nullptr);
    });
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
#endif
    }
}
