add_library(pxc_document STATIC
    piece_table.cpp
    line_index.cpp
    text_document.cpp
)
target_include_directories(pxc_document PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_document PUBLIC pxc_core)
pxc_set_warnings(pxc_document)
set_target_properties(pxc_document PROPERTIES FOLDER "engine")
