pxc_add_test(contract_glyphcss LABEL contract
    SOURCES test_glyphcss.cpp
    LIBS pxc_glyphcss)

pxc_add_test(contract_document LABEL contract
    SOURCES test_document.cpp
    LIBS pxc_document)

pxc_add_test(contract_layout LABEL contract
    SOURCES test_layout.cpp
    LIBS pxc_layout)

pxc_add_test(contract_editor LABEL contract
    SOURCES test_editor.cpp
    LIBS pxc_editor)

pxc_add_test(contract_font_registry LABEL contract
    SOURCES test_font_registry.cpp
    LIBS pxc_fonts)

pxc_add_test(contract_preferences LABEL contract
    SOURCES test_preferences.cpp
    LIBS pxc_storage_win32)

pxc_add_test(contract_html LABEL contract
    SOURCES test_html_parser.cpp test_presentcss.cpp
    LIBS pxc_html)

pxc_add_test(contract_pack_validation LABEL contract
    SOURCES test_pack_validation.cpp
    LIBS pxc_fonts)
