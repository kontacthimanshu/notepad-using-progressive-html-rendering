# pxc_glyphcss: glyph and manifest dialect parsers (independent of presentation CSS, FR-041).
add_library(pxc_glyphcss STATIC
    decl_block.cpp
    glyph_parser.cpp
    manifest_parser.cpp
)
target_include_directories(pxc_glyphcss PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_glyphcss PUBLIC pxc_core)
pxc_set_warnings(pxc_glyphcss)
set_target_properties(pxc_glyphcss PROPERTIES FOLDER "engine")
