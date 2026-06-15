# Phantom

Phantom 是一个 LeviLamina 基岩版悬浮字插件，当前版本 `0.0.1`。它参考 DecentHolograms 的管理模型，但按基岩版客户端能力重新实现：悬浮字通过协议 ID 328 的 Debug Drawer TEXT shape 渲染，不在服务端世界里生成真实实体。

## 功能

- 多行悬浮字，支持 Minecraft 颜色符号。
- 一个悬浮字只发送一个 Primitive TEXT shape，多行文本使用基岩版换行能力合并渲染。
- 按玩家距离和维度动态显示或隐藏。
- 支持自定义维度 ID，不限于主世界、下界、末地。
- 支持动态悬浮字：每行可配置内容池、轮播间隔和玩家变量解析。
- 数据持久化到 `plugins/Phantom/data/holograms.json`。
- `/phantom`、`/hologram`、`/holo` 三个指令入口。
- 基岩版表单管理：创建、列表、编辑文本、动态行、移动到玩家附近、启用、禁用、删除。
- 表单、命令反馈和日志支持 LeviLamina I18n，默认生成中文和英文语言文件。

## 指令

权限：`GameDirectors`。

```text
/phantom
/phantom gui
/phantom list
/phantom reload
/phantom create <name> <text>
/phantom remove <name>
/phantom movehere <name>
/phantom enable <name>
/phantom disable <name>
/phantom append <name> <text>
/phantom setline <name> <index> <text>
/phantom dynamicline <name> <index> <intervalMs> <parseVariables> <text>
/phantom removeline <name> <index>
```

说明：

- `/phantom` 和 `/phantom gui` 打开表单，只能由玩家执行。
- `/phantom create <name> <text>` 在玩家头顶附近创建一行悬浮字。
- 行号从 1 开始。
- 多行文本建议在表单的 Lines 编辑器里维护。
- 动态行的 `text` 可用 `|` 分隔多个内容变体，例如 `A|B|C`。
- `parseVariables` 使用 `0` 或 `1`。
- `movehere` 会把悬浮字移动到执行玩家当前位置上方约 2.2 格。

## 动态悬浮字

表单中进入 `Dynamic lines` 可逐行设置：

- `Content pool`：一行一个内容变体。
- `Update interval ms`：大于 0 时按毫秒轮播内容；为 0 时不轮播。
- `Parse variables`：启用后解析 Phantom 内置变量。

内置变量：

```text
{player}        玩家名
{online}        在线人数
{dimension}     玩家当前维度 ID
{hologram}      悬浮字名称
{line}          当前行号，从 1 开始
{contentIndex}  当前内容池索引，从 0 开始
{x} {y} {z}     玩家整数坐标
```

动态实现保留“每行写什么、每行如何轮播”的管理逻辑，但协议层会把当前各行文本合并为一个带换行的 Primitive TEXT shape。只有当前玩家看到的最终文本实际变化时才重新发包。

## 配置

`plugins/Phantom/config/config.json`：

```json
{
  "version": 1,
  "holograms": true,
  "forms": true,
  "viewDistance": 48.0,
  "lineSpacing": 0.27,
  "refreshIntervalTicks": 10,
  "dynamicRefreshIntervalTicks": 10,
  "language": "zh_CN"
}
```

`plugins/Phantom/data/holograms.json` 保存实际悬浮字：

- `name`：唯一名称，只保留字母、数字、`_`、`-`。
- `dimension`：维度 ID，可填写自定义维度使用的整数 ID。
- `position`：悬浮字的位置。
- `lines`：文本行；每行支持 `text`、`content`、`updateIntervalMs`、`parseVariables`。
- `enabled`：是否显示。
- `viewDistance`：单个悬浮字覆盖默认可视距离；小于等于 0 时使用全局配置。
- `lineSpacing`：保留给管理和未来渲染策略；当前 TEXT shape 使用客户端换行。

## I18n

首次加载插件时会检查 `plugins/Phantom/lang/`：

- 缺少 `en_US.json` 或 `zh_CN.json` 时会生成默认语言文件。
- 语言文件已存在时只加载，不覆盖服主修改。
- 表单发送给玩家时按玩家客户端语言选择文本。
- 控制台、日志或没有玩家语言的场景使用 `config.json` 中的 `language`。

## 构建

```bash
xmake f -y -p windows -a x64 -m release
xmake build Phantom
```

依赖：

- LeviLamina
- SculkCatalystMC/Protocol

## 参考

- DecentHolograms：Java 版成熟悬浮字的功能结构参考。
- BedrockServerClientInterface：基岩版悬浮显示思路参考。
- GMLIB FloatingText：运行时悬浮字管理模型参考。
- GMSidebar：动态内容池、轮播间隔和每玩家缓存刷新策略参考。
- ItemPhys：本仓库内的数据包发包范式参考。
- SculkCatalystMC/Protocol：协议 ID、基础类型和序列化范式参考；ID 328 在当前 BDS/LeviLamina 中按 `DebugDrawerPacket` 线格式写入。
