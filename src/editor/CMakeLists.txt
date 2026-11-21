add_library(pxc_editor STATIC
    editor_controller.cpp
    file_workflow.cpp
)
target_include_directories(pxc_editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pxc_editor PUBLIC pxc_update pxc_layout pxc_storage pxc_document)
pxc_set_warnings(pxc_editor)
set_target_properties(pxc_editor PROPERTIES FOLDER "engine")
