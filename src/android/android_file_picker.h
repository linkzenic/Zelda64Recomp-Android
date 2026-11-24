#pragma once

#include <filesystem>
#include <functional>
#include <list>

namespace zelda64::android {

using FilePickerCallback = std::function<void(bool, const std::filesystem::path&)>;
using MultiFilePickerCallback =
    std::function<void(bool, const std::list<std::filesystem::path>&)>;

void open_file_picker(FilePickerCallback callback);
void open_mod_file_picker(MultiFilePickerCallback callback);
void poll_file_picker();

}
