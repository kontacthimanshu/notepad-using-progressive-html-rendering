# Package layout check (label `package`; run with `ctest --preset x64-release-package`). Builds the ZIP with
# CPack first (fixture), then inspects it.
add_test(NAME package_cpack
    COMMAND ${CMAKE_CPACK_COMMAND} -C $<CONFIG> --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(package_cpack PROPERTIES LABELS package FIXTURES_SETUP pxc_package TIMEOUT 600)

add_test(NAME package_layout
    COMMAND ${CMAKE_COMMAND}
        -DPACKAGE=${PROJECT_SOURCE_DIR}/out/package/PixelCSS-${PROJECT_VERSION}-win64.zip
        -DWORK_DIR=${CMAKE_BINARY_DIR}/package-check
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_package.cmake)
set_tests_properties(package_layout PROPERTIES LABELS package FIXTURES_REQUIRED pxc_package)
