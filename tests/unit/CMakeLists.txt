pxc_add_test(unit_core LABEL unit
    SOURCES core/test_unicode.cpp core/test_geometry.cpp core/test_diagnostics.cpp core/test_css_tokenizer.cpp
    LIBS pxc_core)

pxc_add_test(unit_render LABEL unit
    SOURCES render/test_framebuffer_bmp.cpp render/test_blend.cpp
    LIBS pxc_render)

pxc_add_test(unit_fonts LABEL unit
    SOURCES fonts/test_raster_cache.cpp
    LIBS pxc_fonts)

pxc_add_test(unit_storage LABEL unit
    SOURCES storage/test_text_codec.cpp
    LIBS pxc_storage)

pxc_add_test(unit_update LABEL unit
    SOURCES update/test_dirty_region.cpp
    LIBS pxc_update)
