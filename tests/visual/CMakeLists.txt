pxc_add_test(visual_glyphs LABEL visual
    SOURCES test_glyph_render.cpp test_pack_goldens.cpp test_fallback.cpp
    LIBS pxc_fonts)

pxc_add_test(visual_editor LABEL visual
    SOURCES test_editor_surface.cpp test_scale.cpp
    LIBS pxc_editor)

pxc_add_test(visual_html LABEL visual
    SOURCES test_html_preview.cpp
    LIBS pxc_html pxc_update)
