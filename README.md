# Phantom

Phantom 是一个 LeviLamina 基岩版悬浮字插件。它参考 DecentHolograms 的管理模型，并按基岩版客户端能力重新实现：悬浮字以客户端侧虚拟实体和数据包呈现，不在服务端世界里生成真实实体。

## 功能

- 多行悬浮字，支持 Minecraft 颜色符号。
- 按玩家距离和维度动态显示/隐藏。
- 悬浮字数据持久化到 `plugins/Phantom/data/holograms.json`。
- `/phantom`、`/hologram`、`/holo` 三个指令入口。
- 基岩版表单管理：创建、列表、编辑文本、移动到玩家附近、启用/禁用、删除。
- Sculk Protocol 发包范式：构造协议包、BDS 读回校验、发给指定玩家。

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
/phantom removeline <name> <index>
```

说明：

- `/phantom` 和 `/phantom gui` 打开表单，只能由玩家执行。
- `/phantom create <name> <text>` 在玩家头顶附近创建一行悬浮字。
- 行号从 1 开始。
- 多行文本建议在表单的 Lines 编辑器里维护。
- `movehere` 会把悬浮字移动到执行玩家当前位置上方约 2.2 格。

## 配置

`plugins/Phantom/config/config.json`：

```json
{
  "version": 1,
  "holograms": true,
  "forms": true,
  "viewDistance": 48.0,
  "lineSpacing": 0.27,
  "refreshIntervalTicks": 10
}
```

`plugins/Phantom/data/holograms.json` 保存实际悬浮字：

- `name`：唯一名称，只保留字母、数字、`_`、`-`。
- `dimension`：维度 ID，主世界 `0`、下界 `1`、末地 `2`。
- `position`：第一行的位置。
- `lines`：悬浮字文本行。
- `enabled`：是否显示。
- `viewDistance`：单个悬浮字覆盖默认可视距离；小于等于 0 时使用全局配置。
- `lineSpacing`：单个悬浮字覆盖默认行距；小于等于 0 时使用全局配置。

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
- ItemPhys：本仓库内的数据包发包范式参考。
- SculkCatalystMC/Protocol：协议包结构和序列化实现参考。
