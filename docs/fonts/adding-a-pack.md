# Adding a font pack

A font pack is just a folder. Adding one requires **no engine change and no rebuild** (US6-AS3).

## 1. Copy an existing pack

```powershell
Copy-Item -Recurse fonts\verdana-style fonts\my-pack
```

## 2. Edit `manifest.css`

```css
font-pack {
  --schema: "pixelcss-pack/1";
  --id: "my-pack";                       /* must equal the folder name */
  --display-name: "My Pack";             /* shown in the toolbar dropdown */
  --style-description: "What it looks like";
  --version: "1.0.0";
  --authenticity: "style-interpretation"; /* or "licensed-original" if you hold the rights */
  --origin: "Where the artwork comes from";
  --author: "Your name";
  --license: "OFL-1.1";
  --license-file: "LICENSE.txt";
  --line-height: 15;
  --ascent: 11;
  --descent: 3;
  --coverage: U+0020-007E, U+FFFD;       /* every listed code point needs exactly one file */
  --aliases: "My Pack";                  /* names usable in HTML font-family */
  --order: 10;                           /* dropdown position */
}
```

Replace `LICENSE.txt` with your license text. **Do not trace, sample or convert proprietary fonts**; a pack built
from someone else's typeface needs written permission and `--authenticity: "licensed-original"`.

## 3. Draw or edit glyphs

Change `glyphs/U+XXXX.css` files (and their `--family` to `"my-pack"`). See [glyph-schema.md](glyph-schema.md) for
the format and the tight-ink-box convention.

## 4. Validate

```powershell
pixelcss-tool validate --strict fonts\my-pack     # every glyph, metrics, coverage, provenance
pixelcss-tool distinct fonts                      # must not be a near-copy of another pack (> 60 % identical fails)
pixelcss-tool render --pack my-pack --text "Hello, World! 0123" --scale 2 --out hello.bmp
```

`validate --strict` must report the pack as **Valid** with no errors or warnings.

## 5. Use it

- Restart PixelCSS, or choose **View → Reload Fonts** (Ctrl+Shift+R) — the pack appears in the **Font** dropdown.
- To keep packs outside the install folder: `PixelCSS.exe --fonts-dir D:\my-fonts` (merged with the bundled packs).
- In HTML preview, `font-family: "My Pack"` selects it through `--aliases`.

## 6. (Contributors) add goldens

If the pack is bundled with PixelCSS, regenerate the golden images and review each one by eye:

```powershell
$env:PIXELCSS_UPDATE_GOLDENS = "1"; ctest --preset x64-debug -L visual; Remove-Item Env:PIXELCSS_UPDATE_GOLDENS
```

## How reloading works

Each glyph's identity is the FNV-1a hash of its CSS text. Reloading a pack replaces only glyphs whose hash changed,
bumps the pack revision, and evicts only those raster-cache entries; unchanged glyphs stay cached.
