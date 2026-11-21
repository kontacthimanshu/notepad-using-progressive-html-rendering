# Release packaging (FR-048, research R-03): an offline ZIP containing the executables, every font manifest and
# glyph CSS file, the user README and license texts. The CRT is linked statically, so no redistributable is needed.

install(TARGETS PixelCSS pixelcss-tool RUNTIME DESTINATION .)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/fonts/" DESTINATION fonts)
install(FILES "${PROJECT_SOURCE_DIR}/docs/user/README.txt" DESTINATION .)
install(FILES "${PROJECT_SOURCE_DIR}/fonts/verdana-style/LICENSE.txt"
        DESTINATION LICENSES RENAME PixelCSS-fonts-OFL.txt)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION LICENSES RENAME PixelCSS-LICENSE.txt)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "PixelCSS")
set(CPACK_PACKAGE_VENDOR "PixelCSS contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "PixelCSS-${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/out/package")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PixelCSS Browser - notepad whose text is rendered from CSS-defined pixel glyphs")
include(CPack)
