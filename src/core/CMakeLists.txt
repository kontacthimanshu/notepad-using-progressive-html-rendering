add_library(pxc_core STATIC
    diagnostic.cpp
    geometry.cpp
    unicode.cpp
    css_tokenizer.cpp
)
target_include_directories(pxc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
pxc_set_warnings(pxc_core)
set_target_properties(pxc_core PROPERTIES FOLDER "engine")
