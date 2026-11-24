cmake_minimum_required(VERSION 3.20)

project(Zelda64RecompAndroidNativeMods LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_compile_options(
    -mbranch-protection=standard
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
    "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=."
)
add_link_options(
    "-Wl,-z,max-page-size=16384"
    "-Wl,--strip-all"
)

add_library(ProxyRecomp_KV005 SHARED
    proxy_recomp_kv/src/kv.cpp
    proxy_recomp_kv/src/sqlite3.c
)
target_include_directories(ProxyRecomp_KV005 PRIVATE
    proxy_recomp_kv/include
)
target_link_libraries(ProxyRecomp_KV005 PRIVATE dl m)
set_target_properties(ProxyRecomp_KV005 PROPERTIES PREFIX "")

add_library(yazmt_mm_playermodelmanager_fsmodels_extlib SHARED
    player_model_manager_fsmodels/src/extlib/lib_main.cpp
    player_model_manager_fsmodels/src/extlib/yaz0.cpp
)
target_compile_definitions(yazmt_mm_playermodelmanager_fsmodels_extlib PRIVATE
    YAZMT_PMM_NO_API_IMPORTS
)
target_include_directories(yazmt_mm_playermodelmanager_fsmodels_extlib PRIVATE
    player_model_manager_fsmodels/offline_build
    player_model_manager_fsmodels/include/extlib
    player_model_manager_fsmodels/include/shared
)
target_link_libraries(yazmt_mm_playermodelmanager_fsmodels_extlib PRIVATE dl m)
set_target_properties(yazmt_mm_playermodelmanager_fsmodels_extlib PROPERTIES PREFIX "")
