# Renders an HTML fixture with `pixelcss-tool html` and compares it pixel-exactly with its golden.
file(MAKE_DIRECTORY "${OUT_DIR}")
execute_process(COMMAND "${TOOL}" html --in "${IN}" --out "${OUT_DIR}/out.bmp" --width 480 --fonts-dir "${FONTS_DIR}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "html render failed (${rc}): ${out} ${err}")
endif()
execute_process(COMMAND "${TOOL}" compare "${OUT_DIR}/out.bmp" "${GOLDEN}" RESULT_VARIABLE rc OUTPUT_VARIABLE out)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "compare failed (${rc}): ${out}")
endif()
message(STATUS "tool_html_basic: ${out}")
