# Phantom Architecture

本文档说明 Phantom 的代码结构和基岩版悬浮字实现方式。

## 目标

Phantom 不是 Java 版 DecentHolograms 的逐行翻译，而是面向 LeviLamina 和 Minecraft Bedrock Dedicated Server 的原生实现。核心目标是：

- 管理模型接近 DecentHolograms：命名悬浮字、多行文本、持久化、启停、移动、编辑。
- 显示模型符合基岩版：使用客户端虚拟实体和协议包，不依赖 Bukkit armor stand API。
- 支持自定义维度：维度保存为整数 ID，不假设世界只有 0、1、2 三个维度。
- 动态内容本地化：每行可以拥有内容池、轮播间隔和玩家变量解析。
- 发包实现和 ItemPhys 一致：Sculk Protocol 构包，BDS 原生包读取校验，再发给指定客户端。

## 模块

```text
src/mod/Phantom.*
  插件入口、配置加载、模块启停。

src/phantom/hologram/HologramTypes.*
  悬浮字数据结构、动态行字段、名称规范化、虚拟实体 runtime/unique id 生成。

src/phantom/hologram/HologramService.*
  Manager 核心：仓库、持久化、玩家可见性、动态内容缓存、tick 刷新、虚拟实体增删改包。

src/phantom/net/SculkPacket.h
  Sculk Protocol 发包工具。所有协议包先序列化，再用 BDS MinecraftPackets 读回校验。

src/phantom/commands/Commands.*
  /phantom 指令注册和无表单管理入口。

src/phantom/ui/HologramForms.*
  基岩版表单：主菜单、创建、编辑、静态行编辑、动态行编辑、删除确认。

src/phantom/i18n/I18n.*
  简体中文、繁体中文、英文命令提示。
```

## 数据模型

`Hologram` 表示一个命名悬浮字：

- `name`：唯一名称。
- `dimension`：整数维度 ID，允许自定义维度。
- `position`：第一行位置。
- `lines`：多行内容。
- `enabled`、`viewDistance`、`lineSpacing`：显示控制。

`HologramLine` 表示一行：

- `text`：兼容旧静态字段，始终同步为内容池第一项。
- `content`：动态内容池，一项一个可显示文本。
- `updateIntervalMs`：内容池轮播间隔；为 0 时不轮播。
- `parseVariables`：是否解析 Phantom 内置变量。

旧数据只有 `text` 时会在加载时自动迁移为 `content: [text]`。

## 显示机制

每一行悬浮字对应一个客户端侧虚拟 `minecraft:armor_stand`：

1. `AddActorPacket` 创建虚拟实体。
2. `MetaData` 写入：
   - `Reserved0`：实体 flags，设置 invisible 等标志。
   - `Name`：显示文本。
   - `NametagAlwaysShow`：强制显示名称牌。
   - `NameplateRenderDistanceMax`：客户端名称牌渲染距离。
3. `MoveActorAbsolutePacket` 更新位置。
4. `SetActorDataPacket` 更新文本和名称牌元数据。
5. `RemoveActorPacket` 对玩家隐藏悬浮字。

虚拟实体 ID 由 `hologramName + lineIndex` 哈希生成。这样服务端无需保存真实实体，也不会污染世界存档。

## 可见性

`HologramService::tick()` 按 `refreshIntervalTicks` 和 `dynamicRefreshIntervalTicks` 中较小的值调用 `refreshAll()`：

- 玩家维度 ID 必须和悬浮字维度 ID 一致。
- 玩家距离必须在 `viewDistance` 内。
- 首次可见时发送 `AddActorPacket`。
- 持续可见时发送移动包；文本实际变化时才发送 `SetActorDataPacket`。
- 离开范围或禁用时发送 `RemoveActorPacket`。

玩家断线时只清理服务端可见性缓存；客户端连接已断开，不需要继续发包。

## 动态内容

动态实现参考 GMSidebar 的刷新方式，而不是直接沿用 GMLIB FloatingText 的旧式裸 BinaryStream：

- 每玩家、每 runtime ID 保存 `{currentIndex, renderedText}` 缓存。
- 内容池按 `steady_clock_ms / updateIntervalMs % content.size()` 选择当前项。
- `parseVariables` 启用时按玩家解析 `{player}`、`{online}`、`{dimension}` 等变量。
- 只有缓存文本变化时才发 `SetActorDataPacket`。

这让动态悬浮字可以和玩家上下文绑定，同时避免每 tick 对所有可见行重复发数据包。

## 持久化

配置分两类：

- `config/config.json`：模块级默认值。
- `data/holograms.json`：悬浮字列表。

数据结构保留 `version` 字段，便于后续迁移。维度保存为整数，避免字符串命名空间和本地化名称产生歧义。

## 指令和表单

指令提供自动化和控制台友好入口；表单提供游戏内编辑体验。

- `/phantom` 打开表单。
- `/phantom list`、`reload` 可由控制台执行。
- `/phantom dynamicline` 可设置单行动态内容，多变体用 `|` 分隔。
- 创建、移动、表单 UI 依赖玩家位置，只允许玩家执行。

表单不直接操作文件，而是调用 `HologramService`，保证命令和表单共享同一套校验、保存和刷新逻辑。

## 后续扩展点

- GMLIB PlaceholderAPI：当前没有强依赖，后续可在 `renderVariables()` 中增加可选桥接。
- 点击交互：拦截玩家对虚拟实体 runtime ID 的交互包，映射回悬浮字行。
- 图标或物品行：参考 ItemPhys 的 `AddActorPacket + MobEquipmentPacket + AnimateEntityPacket` 路径。
- 权限细分：把 `GameDirectors` 扩展为更细粒度的命令权限。
