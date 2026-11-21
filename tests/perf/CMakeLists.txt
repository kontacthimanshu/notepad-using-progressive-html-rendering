# Performance regression gate (label `perf`; run with `ctest --preset x64-release-perf`).
# Absolute R-17 budgets apply only with PIXELCSS_REFERENCE_MACHINE=1; the 20% regression check needs a
# measured baseline; resource growth is always enforced.
add_test(NAME perf_regression
    COMMAND pixelcss-bench
        --fonts-dir ${PROJECT_SOURCE_DIR}/fonts
        --fixtures ${PROJECT_SOURCE_DIR}/tests/fixtures
        --scripts ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        --app $<TARGET_FILE:PixelCSS>
        --out ${CMAKE_BINARY_DIR}/perf-results.json
        --check ${CMAKE_CURRENT_SOURCE_DIR}/baseline.json)
set_tests_properties(perf_regression PROPERTIES LABELS perf TIMEOUT 1800)
