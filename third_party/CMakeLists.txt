# Vendored third-party code. doctest 2.4.11 (MIT) is used by tests only.
add_library(pxc_doctest INTERFACE)
target_include_directories(pxc_doctest SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/doctest")
