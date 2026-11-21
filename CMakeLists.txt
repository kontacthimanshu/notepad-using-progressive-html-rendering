cmake_minimum_required(VERSION 3.28)

project(PixelCSS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT: the packaged app needs no VC++ redistributable (research R-03).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MsvcGuard)
include(CompilerWarnings)

enable_testing()

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)

include(Packaging)
