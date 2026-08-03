# Wio Memo 定制系统架构

## 1. 架构决策

MVP 采用 Local-first 单设备架构：Wio Terminal 既是显示/提醒终端，也是局域网 HTTP 服务和数据权威源。这样无需云服务即可工作，断网不影响核心提醒，也符合 SAMD51 和 RTL8720DN 的资源边界。

中文字库确定使用板载 4 MB QSPI Flash，不要求用户安装 microSD 卡。字体采用 WMF1 定长单色点阵格式，索引按 Unicode 码点排序，设备通过 UTF-8 解码和二分查找按需读取字形。microSD 仅保留为未来的数据备份或完整字体扩展选项。

## 2. 分层结构

```text
┌──────────────── Web Browser ────────────────┐
│ Today UI / Editor / Settings / Provisioning │
└────────────────── HTTP/JSON ────────────────┘
                         │
┌──────────────── Wio Terminal ───────────────┐
│ Presentation                               │
│  TFT Views │ Input Mapper │ Buzzer Patterns │
├─────────────────────────────────────────────┤
│ Application                                │
│  TaskService │ ReminderEngine │ SyncService │
│  ProvisioningService │ DeviceStateMachine   │
├─────────────────────────────────────────────┤
│ Domain                                     │
│  Task │ DeviceConfig │ AlertState │ Clock    │
├─────────────────────────────────────────────┤
│ Infrastructure                             │
│  rpcWiFi/WebServer │ NTP │ RTC │ FlashStore │
│  TFT_eSPI │ GPIO/PWM │ Watchdog │ Logger     │
└─────────────────────────────────────────────┘
```

## 3. 运行状态机

```text
BOOT
 ├─ no config ─> PROVISIONING_AP ─> save config ─> RESTART
 └─ configured ─> CONNECTING
                    ├─ success ─> ONLINE
                    └─ timeout ─> OFFLINE_READY

ONLINE/OFFLINE_READY
 ├─ reminder due ─> ALERTING ─> confirmed/snoozed ─> previous state
 ├─ Wi-Fi lost ─> OFFLINE_READY ─> background reconnect ─> ONLINE
 └─ fatal fault ─> watchdog restart ─> BOOT
```

网络连接、Web 请求、屏幕刷新、按键扫描和提醒检查必须采用非阻塞调度。任何联网操作都不能长时间占用主循环，以免错过按键和提醒。

## 4. 建议代码目录

```text
src/
  main.cpp                    组合根与调度器
  domain/task.h               领域模型和校验
  application/task_service.*  CRUD、排序、完成
  application/reminder.*      提醒与稍后提醒状态机
  application/clock.*         UTC、本地时区和校时策略
  infrastructure/storage.*    双槽存储、版本、CRC、迁移
  infrastructure/network.*    STA/AP、退避重连、NTP
  infrastructure/web_api.*    REST 路由、鉴权、输入限制
  presentation/display.*      页面与局部刷新
  presentation/input.*        消抖、短按/长按映射
  presentation/buzzer.*       非阻塞声音序列
web/
  index.html
  app.js
  styles.css
tools/
  embed_web.py                构建时压缩并嵌入 Web 资源
test/
  native/                     时间、排序、提醒、序列化单元测试
```

## 5. 存储设计

- 配置和事项分开存储，避免改任务时反复重写 Wi-Fi 凭据。
- 使用 A/B 双槽：每槽包含 magic、schemaVersion、sequence、payloadLength、CRC32。
- 写入新槽成功并校验后才切换 sequence，掉电时读取最新有效槽。
- 提醒状态必须持久化，确保重启不会重复提醒。
- Wi-Fi 密码不通过普通任务 API 返回；恢复出厂设置才清除。
- 高频状态（当前选择行、屏幕页）只放 RAM，不写 Flash。

## 6. 时间与提醒设计

- 存储和 API 统一使用 UTC Unix 秒；显示时才应用时区。
- RTC 为离线运行时钟，NTP 是校准源；联网启动和每 6 小时校准。
- ReminderEngine 每 200 ms 检查一个排序后的最近提醒，不遍历无关历史项。
- 提醒事件有稳定事件键：`taskId + dueUtc + alertType`，并记录 fired/acknowledged/snoozed。
- NTP 大幅校时后重新计算待触发事件，但不重放已经确认的事件。
- 蜂鸣器使用非阻塞节拍器，不使用 `delay()` 生成整段声音。

## 7. API 草案

| 方法 | 路径 | 用途 |
|---|---|---|
| GET | `/api/v1/device` | 状态、时间、版本、网络 |
| GET | `/api/v1/tasks` | 列表，支持状态筛选 |
| POST | `/api/v1/tasks` | 创建事项 |
| PATCH | `/api/v1/tasks/{id}` | 局部编辑/完成 |
| DELETE | `/api/v1/tasks/{id}` | 删除事项 |
| POST | `/api/v1/tasks/{id}/snooze` | 稍后提醒 |
| POST | `/api/v1/time/sync` | 手动 NTP 校时 |
| GET/PUT | `/api/v1/settings` | 非敏感设置 |
| POST | `/api/v1/provision` | 仅配网模式写入网络配置 |

所有写请求校验 Content-Type、body 大小、字段范围和本地访问令牌。API 采用明确版本号，避免未来 Web 与固件升级不一致。

## 8. 构建与测试框架

- PlatformIO 提供 `device` 与 `native` 两个环境。
- 领域层不依赖 Arduino，可在本机测试提醒边界、跨天、过期、NTP 跳变和存储迁移。
- Web 静态资源单独 lint/test，再在构建阶段压缩为 gzip/PROGMEM。
- 真机测试覆盖：首次配网、错误密码、AP/STA 切换、断网重连、断电恢复、蜂鸣器静音、按键消抖、连续运行 7 天。
- 每个里程碑生成可烧录 BIN/UF2、版本号、变更日志和验收记录。
