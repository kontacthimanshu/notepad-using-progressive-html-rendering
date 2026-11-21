# pxc_presentcss: presentation-CSS subset for the HTML preview (separate from the glyph dialect, FR-041).
add_library(pxc_presentcss STATIC
    presentation_css.cpp
)
target_include_directories(pxc_presentcss PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_presentcss PUBLIC pxc_core)
pxc_set_warnings(pxc_presentcss)
set_target_properties(pxc_presentcss PROPERTIES FOLDER "engine")
