# pxc_render links ONLY pxc_core. It must never depend on glyphcss, fonts, document or any text module
# (contracts/engine-interfaces.md dependency graph).
add_library(pxc_render STATIC
    framebuffer.cpp
    bmp_io.cpp
    renderer.cpp
)
target_include_directories(pxc_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_render PUBLIC pxc_core)
pxc_set_warnings(pxc_render)
set_target_properties(pxc_render PROPERTIES FOLDER "engine")
