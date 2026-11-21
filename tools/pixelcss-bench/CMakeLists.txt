add_executable(pixelcss-bench main.cpp)
target_link_libraries(pixelcss-bench PRIVATE pxc_editor pxc_fonts psapi)
pxc_set_warnings(pixelcss-bench)
set_target_properties(pixelcss-bench PROPERTIES FOLDER "tools")
