# Nexus images

- `key.png` (1920x1080): primary image and thumbnail.
- `banner.png` (1920x480): page header.

Both are drawn in SVG by `art.html` (original artwork, no game asset) and rendered with Microsoft Edge headless:

```
& .\art\render.ps1
```

The script downloads the fonts once into `art/fonts/` (Cinzel and Cormorant Garamond, SIL Open Font License, from Google Fonts; not kept in git). To change a size, edit `--window-size` in the script and the layout in `art.html` (`?key` / `?banner`).
