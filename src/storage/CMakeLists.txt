# Portable part: codecs and load/save logic over the IFileSystem seam (no Win32).
add_library(pxc_storage STATIC
    text_codec.cpp
    file_system.cpp
    text_file.cpp
)
target_include_directories(pxc_storage PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_storage PUBLIC pxc_document pxc_core)
pxc_set_warnings(pxc_storage)
set_target_properties(pxc_storage PROPERTIES FOLDER "engine")

# Win32 implementations of the storage seams (only this library includes <windows.h>).
add_library(pxc_storage_win32 STATIC
    win32/win32_file_system.cpp
    win32/registry_preferences.cpp
)
target_link_libraries(pxc_storage_win32 PUBLIC pxc_storage advapi32)
pxc_set_warnings(pxc_storage_win32)
set_target_properties(pxc_storage_win32 PROPERTIES FOLDER "engine")
