# Nexus images

- `key.png` (1920x1080): primary image and thumbnail.
- `banner.png` (1920x480): page header. Nexus writes the breadcrumb, the mod name and the stats over its lower left, and its buttons over the lower right: `nexus-mock.html` shows the banner under a rough copy of that header, to check nothing important is hidden.

Both are drawn in SVG by `art.html` (original artwork, no game asset) and rendered with Microsoft Edge headless:

```
& .\art\render.ps1
```

The script downloads the fonts once into `art/fonts/` (Cinzel and Cormorant Garamond, SIL Open Font License, from Google Fonts; not kept in git). To change a size, edit `--window-size` in the script and the layout in `art.html` (`?key` / `?banner`).
