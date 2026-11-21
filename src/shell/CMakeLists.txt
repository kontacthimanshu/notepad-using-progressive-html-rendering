configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

# PixelCSS.exe — native Win32 shell. The only engine-adjacent code that includes <windows.h>.
add_executable(PixelCSS WIN32
    main.cpp
    main_window.cpp
    pixel_surface.cpp
    win32_clipboard.cpp
    prompt_service.cpp
    file_commands.cpp
    toolbar.cpp
    info_window.cpp
    about_fonts_dialog.cpp
    diagnostics_dialog.cpp
    stats.cpp
    bench_script.cpp
    about_dialog.cpp
    preview_mode.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    resources.rc
    app.manifest
)
target_link_libraries(PixelCSS PRIVATE
    pxc_editor pxc_html pxc_update pxc_layout pxc_fonts pxc_storage_win32
    comctl32 comdlg32 shell32 ole32 uxtheme psapi)
pxc_set_warnings(PixelCSS)
target_compile_definitions(PixelCSS PRIVATE PXC_VERSION="${PROJECT_VERSION}" PXC_SDK_VERSION="${CMAKE_SYSTEM_VERSION}")
set_target_properties(PixelCSS PROPERTIES
    FOLDER "app"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PixelCSS>")
set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT PixelCSS)

# Font packs next to the executable: <exe-dir>/fonts.
add_custom_command(TARGET PixelCSS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/fonts" "$<TARGET_FILE_DIR:PixelCSS>/fonts"
    VERBATIM)
