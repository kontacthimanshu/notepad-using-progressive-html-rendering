# Verifies the CPack ZIP layout (FR-048, SC-011). Inputs: PACKAGE (path to the .zip), WORK_DIR.
if(NOT EXISTS "${PACKAGE}")
    message(FATAL_ERROR "package not found: ${PACKAGE}")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(ARCHIVE_EXTRACT INPUT "${PACKAGE}" DESTINATION "${WORK_DIR}")

set(errors "")
foreach(required
        PixelCSS.exe
        pixelcss-tool.exe
        README.txt
        LICENSES/PixelCSS-fonts-OFL.txt
        fonts/verdana-style/manifest.css fonts/verdana-style/LICENSE.txt
        fonts/arial-narrow-style/manifest.css fonts/arial-narrow-style/LICENSE.txt
        fonts/comic-style/manifest.css fonts/comic-style/LICENSE.txt)
    if(NOT EXISTS "${WORK_DIR}/${required}")
        string(APPEND errors "missing ${required}\n")
    endif()
endforeach()

set(total 0)
foreach(pack verdana-style arial-narrow-style comic-style)
    file(GLOB glyphs "${WORK_DIR}/fonts/${pack}/glyphs/U+*.css")
    list(LENGTH glyphs n)
    if(NOT n EQUAL 96)
        string(APPEND errors "${pack}: expected 96 glyph files, found ${n}\n")
    endif()
    math(EXPR total "${total} + ${n}")
endforeach()
if(NOT total EQUAL 288)
    string(APPEND errors "expected 288 glyph files in total, found ${total}\n")
endif()

# doctest is a test-only dependency and must not be shipped.
if(EXISTS "${WORK_DIR}/LICENSES/doctest.txt")
    string(APPEND errors "LICENSES/doctest.txt must not be in the package\n")
endif()
# No browser runtime or VC++ redistributable is needed (static CRT).
file(GLOB_RECURSE dlls "${WORK_DIR}/*.dll")
if(dlls)
    string(APPEND errors "unexpected DLLs in package: ${dlls}\n")
endif()

if(errors)
    message(FATAL_ERROR "package layout check failed:\n${errors}")
endif()
message(STATUS "package layout OK: ${total} glyph files, executables, manifests, licenses, README")
