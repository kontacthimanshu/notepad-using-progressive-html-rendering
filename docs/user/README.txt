PixelCSS Browser
================

A lightweight notepad for Windows whose document text is drawn entirely by its own
software renderer, one pixel at a time, from glyphs defined in plain CSS files
(fonts\<pack>\glyphs\U+XXXX.css). No TrueType/OpenType, DirectWrite, GDI text
drawing, WebView2 or browser engine is used for document characters.

Running
-------
Double-click PixelCSS.exe. No installation, runtime or internet connection is needed.
Keep the "fonts" folder next to PixelCSS.exe.

    PixelCSS.exe [file]                      open a .txt file, or a .html file in read-only preview
                                             (View > Page Source, Ctrl+U, shows and edits its HTML)
    PixelCSS.exe --fonts-dir <dir>           also load font packs from another folder
    PixelCSS.exe --no-preferences            do not read or write remembered settings

Fonts
-----
The toolbar offers three bundled pixel font packs: Verdana, Arial Narrow and Comic Sans.
IMPORTANT: these are original pixel artwork drawn in the *style* of those typefaces.
They are NOT the proprietary Verdana, Arial Narrow or Comic Sans fonts and contain no
data derived from them. Help > About Fonts shows each pack's origin, author and license
(SIL Open Font License 1.1, see LICENSES\PixelCSS-fonts-OFL.txt).

Editing
-------
New/Open/Save/Save As (Ctrl+N/O/S, Ctrl+Shift+S), Undo/Redo (Ctrl+Z/Y), Cut/Copy/Paste
(Ctrl+X/C/V), Select All (Ctrl+A), Word Wrap (View menu or toolbar), Reload Fonts
(Ctrl+Shift+R) and a render-statistics overlay (F12). Text files are saved as UTF-8 and
keep their original line endings.

Developer tool
--------------
pixelcss-tool.exe validate --strict fonts     check every font pack
pixelcss-tool.exe render --pack verdana-style --text "Hello" --out hello.bmp

System requirements: Windows 10 or Windows 11, x64.
