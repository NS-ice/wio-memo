# Wio Memo 字体资源

本目录使用 Noto Sans CJK SC 生成适合 Wio Terminal 板载 QSPI Flash 的 WMF1 点阵字库。

| 文件 | 用途 | 字符数 | 大小 |
| --- | --- | ---: | ---: |
| `NotoSansCJKsc-Regular.otf` | 上游字体源文件 | — | — |
| `wio-memo-gb2312-12.wmf` | 小贴士与辅助信息 | 7,445 | 193,602 B |
| `wio-memo-gb2312-16.wmf` | 正文 | 7,445 | 297,832 B |
| `wio-memo-gb2312-20.wmf` | 标题 | 7,445 | 431,842 B |
| `wio-memo-font-bundle.wmf` | 三字号 QSPI 安装包 | — | 956,130 B |

三套字体在 QSPI 中的起始地址分别为 `0x000000`、`0x050000` 和 `0x080000`。

重新生成单个字号：

```powershell
.\.venv\Scripts\python.exe tools\font_pack.py `
  --font assets\fonts\NotoSansCJKsc-Regular.otf `
  --output assets\fonts\wio-memo-gb2312-16.wmf `
  --size 16 `
  --charset gb2312
```

修改任意单字号文件后，使用 `tools/font_bundle.py` 重新打包。字体安装固件运行时上传合并包：

```powershell
.\.venv\Scripts\python.exe tools\font_upload.py `
  --port COM5 `
  --file assets\fonts\wio-memo-font-bundle.wmf
```

将 `COM5` 替换为实际串口。正常固件会分别校验三套 WMF1 头部，缺失的字号使用 LVGL 内置字体回退。

字体来源为 [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)，许可见 `OFL-LICENSE.txt`。
