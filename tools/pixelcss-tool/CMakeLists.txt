add_executable(pixelcss-tool
    main.cpp
    cmd_render.cpp
    cmd_compare.cpp
    cmd_validate.cpp
    cmd_distinct.cpp
    cmd_html.cpp
)
target_link_libraries(pixelcss-tool PRIVATE pxc_fonts pxc_html pxc_update)
pxc_set_warnings(pixelcss-tool)
set_target_properties(pixelcss-tool PROPERTIES FOLDER "tools")
# Ship the font packs next to the tool so `<exe-dir>/fonts` works out of the box.
add_custom_command(TARGET pixelcss-tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/fonts" "$<TARGET_FILE_DIR:pixelcss-tool>/fonts"
    VERBATIM)
