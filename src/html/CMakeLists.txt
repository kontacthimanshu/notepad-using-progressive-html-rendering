# pxc_html: restricted HTML tokenizer, tree builder, cascade and flattening (read-only preview, FR-035..FR-041).
add_library(pxc_html STATIC
    tokenizer.cpp
    tree_builder.cpp
    style.cpp
)
target_include_directories(pxc_html PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_html PUBLIC pxc_presentcss pxc_layout pxc_fonts pxc_core)
pxc_set_warnings(pxc_html)
set_target_properties(pxc_html PROPERTIES FOLDER "engine")
