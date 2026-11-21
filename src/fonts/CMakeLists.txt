# pxc_fonts: rasterize(), FontRegistry (manifest-driven packs, fallback), RasterCache, renderTextLine().
add_library(pxc_fonts STATIC
    rasterize.cpp
    font_pack.cpp
    font_registry.cpp
    raster_cache.cpp
    text_line.cpp
    engine_glyph.cpp
)
target_include_directories(pxc_fonts PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_fonts PUBLIC pxc_glyphcss pxc_render)
pxc_set_warnings(pxc_fonts)
set_target_properties(pxc_fonts PROPERTIES FOLDER "engine")
