add_library(pxc_layout STATIC
    layout_engine.cpp
    viewport.cpp
    block_layout.cpp
)
target_include_directories(pxc_layout PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_layout PUBLIC pxc_fonts pxc_document)
pxc_set_warnings(pxc_layout)
set_target_properties(pxc_layout PROPERTIES FOLDER "engine")
