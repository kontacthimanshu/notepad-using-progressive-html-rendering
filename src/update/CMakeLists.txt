add_library(pxc_update STATIC
    dirty_region.cpp
    update_manager.cpp
    preview_painter.cpp
)
target_include_directories(pxc_update PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_update PUBLIC pxc_layout pxc_fonts pxc_render)
pxc_set_warnings(pxc_update)
set_target_properties(pxc_update PROPERTIES FOLDER "engine")
