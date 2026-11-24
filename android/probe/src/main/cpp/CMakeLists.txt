cmake_minimum_required(VERSION 3.22)
project(ZeldaAndroidProbe LANGUAGES C CXX)

set(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
set(RT64_ROOT "${REPOSITORY_ROOT}/lib/rt64")
set(N64_RUNTIME_ROOT "${REPOSITORY_ROOT}/lib/N64ModernRuntime")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(zelda_android_probe SHARED
    probe_main.cpp
    rt64_probe.cpp
    runtime_probe.cpp
    vulkan_probe.cpp
    ${RT64_ROOT}/src/vulkan/rt64_vulkan.cpp
    ${N64_RUNTIME_ROOT}/ultramodern/src/threadqueue.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
set_source_files_properties(
    ${RT64_ROOT}/src/vulkan/rt64_vulkan.cpp
    PROPERTIES COMPILE_OPTIONS
        "-Wno-nullability-completeness;-Wno-unused-parameter;-Wno-unused-variable;-Wno-unused-function;-Wno-unused-but-set-variable;-Wno-missing-field-initializers;-Wno-deprecated-literal-operator"
)
target_include_directories(zelda_android_probe PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${RT64_ROOT}/src
    ${RT64_ROOT}/src/contrib
    ${RT64_ROOT}/src/contrib/Vulkan-Headers/include
    ${RT64_ROOT}/src/contrib/VulkanMemoryAllocator/include
    ${N64_RUNTIME_ROOT}/N64Recomp/include
    ${N64_RUNTIME_ROOT}/ultramodern/include
    ${N64_RUNTIME_ROOT}/thirdparty
    ${N64_RUNTIME_ROOT}/thirdparty/concurrentqueue
    ${N64_RUNTIME_ROOT}/thirdparty/sse2neon
)
target_compile_options(zelda_android_probe PRIVATE
    -fno-omit-frame-pointer
    -mbranch-protection=standard
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Werror>
)
target_link_options(zelda_android_probe PRIVATE
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
)
target_link_libraries(zelda_android_probe PRIVATE
    android
    log
    vulkan
)
