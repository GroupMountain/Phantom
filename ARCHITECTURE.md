# Phantom Architecture

本文档说明 Phantom 的代码结构和基岩版悬浮字实现。

## 目标

Phantom 不是 Java 版 DecentHolograms 的逐行翻译，而是面向 LeviLamina 和 Minecraft Bedrock Dedicated Server 的原生实现。

- 管理模型接近 DecentHolograms：命名悬浮字、多行文本、持久化、启停、移动、编辑。
- 渲染模型面向基岩版：使用协议 ID 328 的 Debug Drawer TEXT shape，不创建真实实体，也不再模拟 armor stand。
- 支持自定义维度：维度保存为整数 ID，不假设只有 0、1、2 三个维度。
- 支持动态内容：每行可配置内容池、轮播间隔和玩家变量解析。
- 发包实现对齐 ItemPhys 范式：Sculk Protocol 构包，BDS 原生包读回校验，再发送给指定玩家。

## 模块

```text
src/mod/Phantom.*
  插件入口、配置加载、模块启停。
src/phantom/hologram/HologramTypes.*
  悬浮字数据结构、动态行字段、名称规范化、shape network id 生成。
src/phantom/hologram/HologramService.*
  Manager 核心：仓库、持久化、玩家可见性、动态内容缓存、tick 刷新、PrimitiveShapes 发包。
src/phantom/net/SculkPacket.h
  Sculk Protocol 发包工具。所有协议包先序列化，再用 BDS MinecraftPackets 读回校验。
src/phantom/commands/Commands.*
  /phantom 指令注册和无表单管理入口。指令反馈按执行者语言本地化。
src/phantom/ui/HologramForms.*
  基岩版表单：主菜单、创建、编辑、静态行、动态行、高级选项、删除确认。
src/phantom/i18n/I18n.*
  默认语言文件生成、语言目录加载、locale 规范化。
```

## 数据模型

`Hologram` 表示一个命名悬浮字：

- `name`：唯一名称。
- `dimension`：整数维度 ID，允许自定义维度。
- `position`：整个悬浮字的位置。
- `lines`：多行内容。协议层会把它们用 `\n` 合并成一个 TEXT shape。
- `enabled`、`viewDistance`、`lineSpacing`：显示控制。`lineSpacing` 保留给管理和未来渲染策略，当前 Primitive TEXT shape 使用客户端换行。

`HologramLine` 表示一行：

- `text`：兼容旧静态字段，始终同步为内容池第一项。
- `content`：动态内容池，一项一个候选文本。
- `updateIntervalMs`：内容池轮播间隔；为 0 时不轮播。
- `parseVariables`：是否解析 Phantom 内置变量。

旧数据只有 `text` 时会在加载时自动迁移为 `content: [text]`。

## 显示机制

每个悬浮字对每个可见玩家只对应一个客户端侧 Debug Drawer TEXT shape：

1. `HologramService` 逐行解析文本，包括动态内容池和玩家变量。
2. 将所有行用 `\n` 合并为一个字符串。
3. 使用协议 ID 328 发送一个 TEXT shape。当前 LeviLamina/BDS 将该包命名为 `DebugDrawerPacket`，Sculk Protocol 中同一 ID 命名为 `PrimitiveShapes`：
   - `mNetworkId`：由 `hologramName + 0` 稳定哈希生成。
   - `mLocation`：悬浮字位置。
   - `mDimensionId`：悬浮字维度 ID。
   - `TextDataPayload`：只写合并后的文本字符串。
4. 更新文本时复用相同 `mNetworkId` 重新发送 TEXT shape。
5. 删除或隐藏时发送同一个 `mNetworkId`、空 `mType`、空 payload 的 shape remove 包。

该设计避免了旧实现的虚拟实体链路，也避免了 `MoveActorAbsolutePacket` 在当前 BDS/Protocol 组合下反复校验失败的问题。注意当前运行时的 `ShapeDataPayload` 字段顺序和 `TextDataPayload` 与部分第三方 `PrimitiveShapesPacket` 文档不同，Phantom 的写包器按 LeviLamina/BDS 头文件的 Debug Drawer 线格式写入。

## 可见性

`HologramService::tick()` 按 `refreshIntervalTicks` 和 `dynamicRefreshIntervalTicks` 中较小的值调用 `refreshAll()`：

- 玩家维度 ID 必须和悬浮字维度 ID 一致。
- 玩家距离必须在 `viewDistance` 内。
- 首次可见时发送一个 TEXT shape。
- 持续可见时逐行解析，只有合并后的文本实际变化才重新发送。
- 离开范围或禁用时发送 remove shape。

玩家断线时只清理服务端可见性缓存；客户端连接已经断开，不需要继续发包。

## 动态内容

动态实现参考 GMSidebar 的“内容池 + update interval + 每玩家缓存”思路：

- 每玩家、每 hologram shape id 保存最后一次合并后的文本。
- 每行按 `steady_clock_ms / updateIntervalMs % content.size()` 选择当前内容。
- `parseVariables` 启用时按玩家解析 `{player}`、`{online}`、`{dimension}`、`{hologram}`、`{line}`、`{contentIndex}`、`{x}`、`{y}`、`{z}`。
- 只在最终文本变化时发包，避免每 tick 对所有可见悬浮字重复发送。

## 持久化和 I18n

配置分三类：

- `config/config.json`：模块级默认值。
- `data/holograms.json`：悬浮字列表。
- `lang/en_US.json`、`lang/zh_CN.json`：表单、命令反馈和日志文本。

语言文件采用“缺失才生成”的策略：如果服主已经修改过 `lang/*.json`，Phantom 只会加载，不会覆盖。玩家表单按玩家客户端 locale 取文本；控制台、日志和其他无玩家场景使用 `config.json` 的 `language`。

## 指令和表单

- `/phantom` 打开表单。
- `/phantom list`、`/phantom reload` 可由控制台执行。
- `/phantom dynamicline` 可设置单行动态内容，多个变体用 `|` 分隔。
- 创建、移动、表单 UI 依赖玩家位置，只允许玩家执行。

表单不直接操作文件，而是调用 `HologramService`，保证命令和表单共享同一套校验、保存和刷新逻辑。

## 后续扩展点

- PlaceholderAPI：后续可在 `renderVariables()` 中增加可选桥接。
- 点击交互：PrimitiveShapes 本身不是实体，若需要点击交互应设计独立的客户端交互映射，而不是依赖实体 runtime id。
- 图标或物品行：可以参考 ItemPhys 的实体和装备包路径，但应与文本 shape 渲染层分离。
- 权限细分：把 `GameDirectors` 扩展为更细粒度的命令权限。
