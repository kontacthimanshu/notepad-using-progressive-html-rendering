include("${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake")

# Shared test support library (golden harness, fakes, CRT leak-check listener).
add_library(pxc_testsupport STATIC
    test_main.cpp
    support/golden.cpp
    support/leak_check.cpp
)
target_include_directories(pxc_testsupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pxc_testsupport PUBLIC pxc_doctest pxc_render pxc_core)
target_compile_definitions(pxc_testsupport PUBLIC
    PXC_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    PXC_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    PXC_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts"
    PXC_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    PXC_GOLDEN_FAILURE_DIR="${CMAKE_BINARY_DIR}/golden-failures"
)
pxc_set_warnings(pxc_testsupport)

# pxc_add_test(<name> LABEL <unit|contract|integration|visual|perf> SOURCES ... [LIBS ...])
function(pxc_add_test name)
    cmake_parse_arguments(T "" "LABEL" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE pxc_testsupport ${T_LIBS})
    pxc_set_warnings(${name})
    set_target_properties(${name} PROPERTIES FOLDER "tests/${T_LABEL}")
    doctest_discover_tests(${name}
        PROPERTIES LABELS ${T_LABEL}
        TEST_PREFIX "${name}/"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

add_subdirectory(unit)
add_subdirectory(contract)
add_subdirectory(integration)
add_subdirectory(visual)
add_subdirectory(perf)
add_subdirectory(package)
