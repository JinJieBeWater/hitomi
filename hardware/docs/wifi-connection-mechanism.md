# Hardware Wi-Fi 连接机制

本文档说明 `hardware/` 当前这套 Wi-Fi 配网与连接机制是如何工作的，
重点回答三个问题：

1. 设备保存哪些 Wi-Fi 配置与连接元数据
2. 设备启动或断线后如何决定先连哪个网络
3. 为什么这套机制比“先扫网再一个个试”更快、更稳

## 设计目标

当前机制的目标不是做“最复杂”的联网框架，而是解决固件里的几个真实问题：

- 避免启动时因为固定扫描或固定节流造成明显滞后
- 避免错误密码的 profile 反复阻塞正确 profile
- 在多 Wi-Fi profile 场景下优先命中最有可能成功的网络
- 保留对隐藏 SSID 的支持
- 让重连行为更多由明确状态机和 Wi-Fi 事件驱动，而不是黑盒自动重连

## 配网输入

设备当前通过 USB CDC JSON 命令接收配网输入，核心命令是：

```json
{"type":"set_wifi_profiles","profiles":[{"ssid":"Lab","password":"secret","priority":5}]}
```

每个 profile 由以下逻辑字段构成：

- `ssid`
- `password`
- `priority`
- `lastSuccessAt`
- `lastSuccessBssid`
- `lastSuccessChannel`
- `disabled`

其中：

- `ssid/password/priority` 由运维或 Web Serial 配网输入决定
- `lastSuccess*` 由设备在成功联网后自动回填
- `disabled` 用于显式禁用某条 profile

## 持久化策略

Wi-Fi profile 属于设备运行配置的一部分，会随 `deviceConfig` 一起持久化。

当前会持久化的 Wi-Fi 连接元数据包括：

- `lastSuccessAt`
- `lastSuccessBssid`
- `lastSuccessChannel`

这些字段的作用是让设备在下次启动时优先做“定向连接”：

- 如果已知上次连上的 AP 的 `BSSID/channel`
- 就优先按该目标发起连接
- 这样比重新全信道搜索更快

当前不会持久化的内容：

- auth failure 冷却状态
- 本轮扫描候选列表
- 本轮 profile 失败原因统计

这些属于运行时策略状态，掉电后可重建，不需要写入持久化层。

## 连接总流程

当前连接流程是一个分阶段状态机：

### 1. 启动后先尝试 `last-known-good`

设备会先从已配置 profile 中选出“最值得先试”的 profile，排序基础是：

- `priority` 高的优先
- `lastSuccessAt` 新的优先

如果该 profile 带有 `lastSuccessBssid/lastSuccessChannel`，则优先使用：

- `WiFi.begin(ssid, password, channel, bssid)`

这是最快的路径，目标是直接命中上次成功的 AP，而不是先做完整扫描。

### 2. 如果直连失败，做一次异步扫描

如果 last-known-good 定向连接失败，设备会发起一次异步扫描。

扫描只负责回答这些问题：

- 哪些 SSID 当前可见
- 对应 AP 的 `RSSI`
- `auth mode`
- `BSSID`
- `channel`

扫描**不能**回答：

- 这个 profile 的密码是否正确

密码正确性只能通过一次真实的关联/认证过程得到。

### 3. 扫描完成后构建候选集

扫描完成后，设备不会对每个 profile 盲试所有 AP。

它会做两件事：

1. 对每个已配置 profile，只保留一个“当前最优可见候选 AP”
2. 再对这些候选 profile 做全局排序

单个 profile 的“最优可见候选 AP”比较维度：

- 是否命中 `lastSuccessBssid`
- 是否命中 `lastSuccessChannel`
- `RSSI` 更高
- `channel` 更稳定

最终候选 profile 的全局排序维度：

- profile `priority`
- `lastSuccessAt`
- 是否命中 last-known-good BSSID
- `RSSI`

### 4. 候选连接尝试

设备会按排序后的候选集逐个发起连接，但不是重新扫描一次再试一次，
而是在**同一轮扫描结果**上依次消费候选。

这样做的好处是：

- 不会为每个 profile 重复付出扫描代价
- 多 profile 时能优先尝试最有希望成功的网络

### 5. 单次 fallback 直连

如果扫描没有得到任何可用候选，或者候选集被耗尽，设备会做一次
“按配置直接连”的 fallback。

这一层存在的原因是：

- 隐藏 SSID 在普通扫描里不可见
- 扫描结果可能瞬时为空
- 某些 AP 在扫描和连接之间状态可能变化

因此，完全依赖扫描会牺牲隐藏 SSID 和瞬时波动网络的可用性。

## 失败原因处理

当前机制会区分连接失败原因，而不是一律立即重试。

### Auth 类失败

以下失败原因被视为“更像密码或鉴权问题”：

- `AUTH_FAIL`
- `HANDSHAKE_TIMEOUT`
- `4WAY_HANDSHAKE_TIMEOUT`
- `802_1X_AUTH_FAILED`

对这类失败：

- 当前 profile 会进入运行时冷却
- 默认冷却时间为 `2 分钟`
- 同一启动周期内，设备不会让错误 profile 反复阻塞其他 profile

### 非 auth 类失败

例如：

- `NO_AP_FOUND`
- 一般断链或超时

这些失败不会触发长期冷却，而是进入下一步候选或下一轮重试。

## 成功后的状态更新

当设备收到 `GOT_IP` 事件后，会执行以下动作：

- 标记 Wi-Fi 已连接
- 记录当前 `SSID`
- 回填该 profile 的：
  - `lastSuccessAt`
  - `lastSuccessBssid`
  - `lastSuccessChannel`
- 清空该 profile 的运行时冷却
- 重置后续 API probe / activation / sync 调度

因此，下次启动时设备能优先复用本次的成功路径。

## 为什么不是“直接探测出可用网络”

从 Wi-Fi 协议和 ESP32 驱动能力上说，扫描只能发现：

- AP 是否存在
- 信号强弱
- 加密类型
- `BSSID/channel`

但扫描本身无法证明：

- 这组 `SSID/password` 一定可用

原因是密码是否正确，必须经过真实的：

- 关联
- 认证
- 四次握手

因此，工程上的正确做法不是“只扫描、不连接”，而是：

- 先扫描筛候选
- 再把连接尝试次数压到最少

当前机制就是这个思路。

## 当前限制

当前实现仍有边界：

- 错误密码的 profile 仍然会消耗一次真实连接尝试，无法在扫描阶段提前判定
- 隐藏 SSID 仍依赖 fallback 直连
- auth failure 冷却是运行时内存态，重启后不会保留
- 当前只保留每个 profile 的“最佳单个候选 AP”，不做更复杂的多 BSSID 历史质量模型

## 关键调参点

相关常量位于：

- `hardware/include/board/app_config.hpp`

当前关键参数包括：

- `kWifiConnectRetryIntervalMs`
- `kWifiConnectAttemptTimeoutMs`
- `kWifiScanRetryIntervalMs`
- `kWifiAuthFailureCooldownMs`

如果后续要做现场优化，建议围绕这些参数结合真机日志调节，而不是先改更大结构。

## 建议的后续增强

如果后续需要继续工程化，可以在当前机制上继续加：

- 启动阶段 `boot -> connect -> got_ip` 毫秒级埋点
- 按 profile 统计失败次数与最近失败时间
- 对 `NO_AP_FOUND` 和 `AUTH_FAIL` 使用不同退避策略
- 在管理端暴露 last-known-good 与最近失败原因，便于排障
- 在重启后保留部分失败统计，用于更稳的 profile 降权

当前这版机制的定位是：

- 先解决“连接明显滞后”和“坏密码拖慢好网络”的问题
- 同时保持实现边界清晰、易于继续演进
