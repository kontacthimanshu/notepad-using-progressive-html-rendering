# US1 independent test: render "A" with pixelcss-tool and compare it pixel-exactly with the golden.
# Inputs: TOOL, GOLDEN, OUT_DIR, FONTS_DIR
file(MAKE_DIRECTORY "${OUT_DIR}")
execute_process(COMMAND "${TOOL}" render --pack verdana-style --text A --scale 1 --fonts-dir "${FONTS_DIR}"
                        --out "${OUT_DIR}/A.bmp"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "render failed (${rc}): ${out} ${err}")
endif()
execute_process(COMMAND "${TOOL}" compare "${OUT_DIR}/A.bmp" "${GOLDEN}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "compare failed (${rc}): ${out} ${err}")
endif()
message(STATUS "tool_render_A: ${out}")
