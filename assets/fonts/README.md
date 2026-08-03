# Wio Memo font assets

- `NotoSansCJKsc-Regular.otf`: source font from the official
  [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk) repository.
- `OFL-LICENSE.txt`: the upstream SIL Open Font License.
- `wio-memo-gb2312-12.wmf`: small UI hints, 7,445 GB2312 glyphs.
- `wio-memo-gb2312-16.wmf`: body copy, 7,445 GB2312 glyphs.
- `wio-memo-gb2312-20.wmf`: headings, 7,445 GB2312 glyphs.
- `wio-memo-font-bundle.wmf`: installable fixed-layout QSPI image containing all
  three sizes at `0x000000`, `0x050000`, and `0x080000`.

Rebuild the device image from the project root:

```powershell
.\.venv\Scripts\python.exe tools\font_pack.py `
  --font assets\fonts\NotoSansCJKsc-Regular.otf `
  --output assets\fonts\wio-memo-gb2312-16.wmf `
  --size 16 `
  --charset gb2312
```

Upload it while the `font_installer` firmware is running:

```powershell
.\.venv\Scripts\python.exe tools\font_upload.py `
  --port COM3 `
  --file assets\fonts\wio-memo-font-bundle.wmf
```

Use `tools/font_bundle.py` after rebuilding any individual size. The normal
firmware validates every WMF header independently and falls back per size.
