# FR-042: the primary supported builds MUST use the MSVC compiler (cl.exe).
# Rejects GCC, MinGW, Clang and clang-cl, and 32-bit targets.

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "PixelCSS must be compiled with MSVC (cl.exe). Detected compiler: "
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}. "
        "Use a Visual Studio generator preset, e.g. `cmake --preset x64-debug`.")
endif()

if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR
        "PixelCSS requires the cl.exe front end; clang-cl and other MSVC-compatible "
        "front ends are not supported (front end: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PixelCSS supports x64 only. Configure with -A x64.")
endif()

message(STATUS "PixelCSS: compiler = MSVC ${CMAKE_CXX_COMPILER_VERSION} (cl.exe)")
