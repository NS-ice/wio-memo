# 板载 QSPI 中文字库工作流

## 方案概览

Wio Memo 不需要 SD 卡。中文字库写入 Wio Terminal 板载 4 MB W25Q32 QSPI Flash，固件启动后验证 WMF1 头部，并通过 UTF‑8 解码、Unicode 二分查找和 LVGL 字体回调按需读取字形。

当前仓库包含三套 7,445 字 GB2312 字库：

| 文件 | 字号 | 大小 |
| --- | --- | ---: |
| `wio-memo-gb2312-12.wmf` | 12 px | 193,602 B |
| `wio-memo-gb2312-16.wmf` | 16 px | 297,832 B |
| `wio-memo-gb2312-20.wmf` | 20 px | 431,842 B |
| `wio-memo-font-bundle.wmf` | 三套合并 | 956,130 B（约 934 KiB） |

字库只保存字形，不包含任务、会议或 Wi‑Fi 密码。

## 首次安装

### 1. 进入下载模式

保持 USB 连接，快速双击 Wio Terminal 复位键。用以下命令确认串口：

```powershell
.\.venv\Scripts\pio.exe device list
```

### 2. 写入字体安装固件

```powershell
.\.venv\Scripts\pio.exe run -e font_installer -t upload
```

### 3. 上传合并字体包

```powershell
python tools\font_upload.py --port COM5 --file assets\fonts\wio-memo-font-bundle.wmf
```

将 `COM5` 替换为实际端口。上传工具分块写入，并读回校验 CRC32。

### 4. 恢复主固件

```powershell
.\.venv\Scripts\pio.exe run -e seeed_wio_terminal -t upload
```

主固件上传不会主动擦除 QSPI 字库。串口显示 `QSPI fonts 12/16/20 ready` 表示三套字体均验证成功。

## 重新生成字库

字体源文件已放在 `assets/fonts/NotoSansCJKsc-Regular.otf`。示例：

```powershell
python -m pip install -r tools\requirements.txt
python tools\font_pack.py `
  --font assets\fonts\NotoSansCJKsc-Regular.otf `
  --output assets\fonts\wio-memo-gb2312-16.wmf `
  --size 16 `
  --charset gb2312
```

如果修改任意单字号文件，还需要按工具说明重新生成合并包，然后重新执行安装流程。

## 常见问题

- **PlatformIO 找不到端口**：设备不在下载模式；重新双击复位并再次运行 `device list`。
- **只显示英文或方框**：字体包未写入、地址不匹配或 CRC 校验失败。
- **需要 SD 卡吗**：不需要；当前字体包不到 1 MB，板载 4 MB QSPI 足够。
- **能否放完整 CJK 字库**：空间和查询成本都会显著增加；当前产品优先覆盖 GB2312 常用字。
- **字体许可证**：Noto Sans CJK SC 使用 SIL OFL，许可文件见 `assets/fonts/OFL-LICENSE.txt`。
