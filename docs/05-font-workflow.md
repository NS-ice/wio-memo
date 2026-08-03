# 板载 QSPI 中文字库工作流

## 存储布局

Wio Memo 将字库写入 Wio Terminal 的 4 MB W25Q32 QSPI Flash 起始地址。字库使用 WMF1 格式：32 字节头、按 Unicode 码点排序的 8 字节索引、定长 1-bit 点阵数据。固件启动后验证头部，再用二分查找按需读取单个字形。

当前 16×16 GB2312 测试结果为 7,445 个字符、约 298 KB。板载 QSPI 空间充足，microSD 不参与字体运行。

## 1. 选择开源字体

推荐使用 SIL Open Font License 的 Noto Sans CJK SC 或同类开源字体。字体源文件不提交到本仓库；生成的 WMF 文件应保留原字体许可说明。

## 2. 生成 WMF1

```powershell
python -m pip install -r tools/requirements.txt
python tools/font_pack.py `
  --font C:\path\to\NotoSansCJKsc-Regular.otf `
  --output assets\font-16.wmf `
  --size 16 `
  --charset gb2312
```

## 3. 烧录字体安装器

```powershell
pio run -e font_installer -t upload
```

## 4. 上传字库

```powershell
python tools/font_upload.py --port COM5 --file assets\font-16.wmf
```

上传器会按 512 字节分块写入，随后从 QSPI 读回并验证整个文件 CRC32。

## 5. 恢复正常固件

```powershell
pio run -e seeed_wio_terminal -t upload
```

正常固件不会擦除 QSPI 字库。串口输出 `QSPI font ready` 表示字库头验证成功；如果显示 `QSPI font missing`，中文使用方框占位，英文仍正常显示。

## 注意

- 字库安装器会擦除 QSPI 起始区域；当前版本尚未把其他业务数据放入 QSPI。
- 更换字库必须重新执行安装器流程。
- 字库只保存字形，不包含用户事项和 Wi-Fi 密码。
