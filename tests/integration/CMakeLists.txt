# --- pixelcss-tool end-to-end checks (US1) ---
add_test(NAME tool_render_A
    COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:pixelcss-tool>
        -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden/glyphs/verdana-style/U+0041.s1.bmp
        -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tool_render_A
        -DFONTS_DIR=${PROJECT_SOURCE_DIR}/fonts
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_tool_render_A.cmake)
set_tests_properties(tool_render_A PROPERTIES LABELS integration)

add_test(NAME tool_validate_invalid_glyph
    COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:pixelcss-tool>
        "-DARGS=validate;${PROJECT_SOURCE_DIR}/tests/fixtures/packs/bad-row"
        -DEXPECT_RC=1
        -DEXPECT_REGEX=E-GLYPH-ROW-LENGTH
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_expect_failure.cmake)
set_tests_properties(tool_validate_invalid_glyph PROPERTIES LABELS integration)

# --- US2 integration ---
pxc_add_test(integration_files LABEL integration
    SOURCES test_file_failures.cpp test_editor_roundtrip.cpp
    LIBS pxc_editor pxc_storage_win32)

# --- US3 integration ---
pxc_add_test(integration_font_switch LABEL integration
    SOURCES test_font_switch.cpp
    LIBS pxc_editor)

add_test(NAME fonts_distinct COMMAND pixelcss-tool distinct ${PROJECT_SOURCE_DIR}/fonts)
set_tests_properties(fonts_distinct PROPERTIES LABELS integration)

# --- US4 integration ---
pxc_add_test(integration_incremental LABEL integration
    SOURCES test_incremental_paint.cpp test_progressive_open.cpp
    LIBS pxc_editor)

# --- US7 integration (needs the real PixelCSS.exe) ---
pxc_add_test(integration_binary LABEL integration
    SOURCES test_binary_imports.cpp test_resource_cleanup.cpp
    LIBS pxc_editor pxc_storage_win32)
target_compile_definitions(integration_binary PRIVATE PXC_APP_EXE="$<TARGET_FILE:PixelCSS>")
add_dependencies(integration_binary PixelCSS)

# --- US5: headless HTML preview through the tool matches the golden ---
add_test(NAME tool_html_basic
    COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:pixelcss-tool>
        -DIN=${PROJECT_SOURCE_DIR}/tests/fixtures/html/basic.html
        -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden/html/basic.bmp
        -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tool_html_basic
        -DFONTS_DIR=${PROJECT_SOURCE_DIR}/fonts
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_tool_html.cmake)
set_tests_properties(tool_html_basic PROPERTIES LABELS integration)

# --- US6: pack reload and strict validation of the bundled packs ---
pxc_add_test(integration_pack_reload LABEL integration
    SOURCES test_pack_reload.cpp
    LIBS pxc_fonts)

add_test(NAME validate_bundled_fonts COMMAND pixelcss-tool validate --strict ${PROJECT_SOURCE_DIR}/fonts)
set_tests_properties(validate_bundled_fonts PROPERTIES LABELS integration
    PASS_REGULAR_EXPRESSION "total ASCII glyph files: 285; 0 error\(s\), 0 warning\(s\)")

# --- Polish: 10M-character document ---
pxc_add_test(integration_large_document LABEL integration
    SOURCES test_large_document.cpp
    LIBS pxc_editor psapi)
