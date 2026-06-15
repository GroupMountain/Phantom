#include "phantom/i18n/I18n.h"

#include "ll/api/i18n/I18n.h"

#include <filesystem>
#include <fstream>

namespace phantom::i18n {
namespace {

constexpr char kEnUs[] = R"json({
  "phantom.command.player_only": "This command can only be used by a player.",
  "phantom.command.not_found": "Hologram not found.",
  "phantom.command.exists": "A hologram with that name already exists.",
  "phantom.command.created": "Hologram created.",
  "phantom.command.deleted": "Hologram deleted.",
  "phantom.command.updated": "Hologram updated.",
  "phantom.command.reloaded": "Holograms reloaded.",
  "phantom.command.empty": "No holograms have been created.",
  "phantom.command.description": "Manage Phantom holograms",
  "phantom.command.requires_more_args": "This action requires more arguments.",
  "phantom.command.unsupported_named_action": "Unsupported named action.",
  "phantom.command.unsupported_text_action": "Unsupported text action.",
  "phantom.command.unsupported_line_action": "Unsupported line action.",
  "phantom.command.line_index_starts_at_one": "Line index starts at 1.",
  "phantom.command.list_header": "Phantom holograms ({count}):",
  "phantom.command.list_item": "- {name} [{state}] lines={lines} dim={dimension} pos=({x}, {y}, {z})",
  "phantom.command.state_on": "on",
  "phantom.command.state_off": "off",

  "phantom.log.config_missing": "Config file was missing, invalid, or upgraded: {path}",
  "phantom.log.i18n_failed": "Failed to prepare or load Phantom i18n files",
  "phantom.log.store_missing": "Hologram store was missing or invalid: {path}",
  "phantom.log.packet_validation_failed": "Sculk packet validation failed for {packet} ({id})",
  "phantom.log.loading": "Loading Phantom...",
  "phantom.log.enabling": "Enabling Phantom...",
  "phantom.log.disabling": "Disabling Phantom...",

  "phantom.form.main.title": "Phantom",
  "phantom.form.main.content": "Hologram manager",
  "phantom.form.main.create": "+ Create",
  "phantom.form.main.reload": "Reload",
  "phantom.form.state.disabled": "disabled",
  "phantom.form.state.dynamic": "dynamic",
  "phantom.form.state.static": "static",
  "phantom.form.empty": "empty",
  "phantom.form.back": "< Back",

  "phantom.form.create.title": "Create hologram",
  "phantom.form.field.name": "Name",
  "phantom.form.field.lines": "Lines",
  "phantom.form.field.lines.placeholder": "One line per row",
  "phantom.form.field.dimension": "Dimension ID",
  "phantom.form.field.dimension.placeholder": "0, 1, 2, or a custom dimension id",
  "phantom.form.field.dimension.custom": "Supports custom dimensions",
  "phantom.form.field.enabled": "Enabled",

  "phantom.form.edit.title": "Edit {name}",
  "phantom.form.edit.content": "Lines: {lines}\\nDimension: {dimension}",
  "phantom.form.edit.lines": "Edit lines",
  "phantom.form.edit.dynamic_lines": "Dynamic lines",
  "phantom.form.edit.move_to_me": "Move to me",
  "phantom.form.edit.enable": "Enable",
  "phantom.form.edit.disable": "Disable",
  "phantom.form.edit.advanced": "Advanced",
  "phantom.form.edit.delete": "Delete",

  "phantom.form.advanced.title": "Advanced {name}",
  "phantom.form.field.view_distance": "View distance",
  "phantom.form.field.line_spacing": "Line spacing",

  "phantom.form.lines.title": "Lines {name}",

  "phantom.form.dynamic.title": "Dynamic lines {name}",
  "phantom.form.dynamic.content": "Select a line to configure dynamic content.",
  "phantom.form.dynamic.line_title": "Dynamic line {line}",
  "phantom.form.dynamic.variables": "Variables: {player}, {online}, {dimension}, {hologram}, {line}, {contentIndex}, {x}, {y}, {z}",
  "phantom.form.field.content_pool": "Content pool",
  "phantom.form.field.content_pool.placeholder": "One variant per row",
  "phantom.form.field.update_interval": "Update interval ms",
  "phantom.form.field.update_interval.placeholder": "0 = no rotation",
  "phantom.form.field.parse_variables": "Parse variables",

  "phantom.form.delete.title": "Delete {name}",
  "phantom.form.delete.content": "This removes the hologram from storage.",
  "phantom.form.delete.confirm": "Delete",
  "phantom.form.delete.cancel": "Cancel"
}
)json";

constexpr char kZhCn[] = R"json({
  "phantom.command.player_only": "\u8be5\u547d\u4ee4\u53ea\u80fd\u7531\u73a9\u5bb6\u6267\u884c\u3002",
  "phantom.command.not_found": "\u627e\u4e0d\u5230\u6307\u5b9a\u60ac\u6d6e\u5b57\u3002",
  "phantom.command.exists": "\u540c\u540d\u60ac\u6d6e\u5b57\u5df2\u7ecf\u5b58\u5728\u3002",
  "phantom.command.created": "\u60ac\u6d6e\u5b57\u5df2\u521b\u5efa\u3002",
  "phantom.command.deleted": "\u60ac\u6d6e\u5b57\u5df2\u5220\u9664\u3002",
  "phantom.command.updated": "\u60ac\u6d6e\u5b57\u5df2\u66f4\u65b0\u3002",
  "phantom.command.reloaded": "\u60ac\u6d6e\u5b57\u5df2\u91cd\u8f7d\u3002",
  "phantom.command.empty": "\u8fd8\u6ca1\u6709\u521b\u5efa\u4efb\u4f55\u60ac\u6d6e\u5b57\u3002",
  "phantom.command.description": "\u7ba1\u7406 Phantom \u60ac\u6d6e\u5b57",
  "phantom.command.requires_more_args": "\u8be5\u64cd\u4f5c\u9700\u8981\u66f4\u591a\u53c2\u6570\u3002",
  "phantom.command.unsupported_named_action": "\u4e0d\u652f\u6301\u7684\u547d\u540d\u64cd\u4f5c\u3002",
  "phantom.command.unsupported_text_action": "\u4e0d\u652f\u6301\u7684\u6587\u672c\u64cd\u4f5c\u3002",
  "phantom.command.unsupported_line_action": "\u4e0d\u652f\u6301\u7684\u884c\u64cd\u4f5c\u3002",
  "phantom.command.line_index_starts_at_one": "\u884c\u53f7\u4ece 1 \u5f00\u59cb\u3002",
  "phantom.command.list_header": "Phantom \u60ac\u6d6e\u5b57\uff08{count}\uff09\uff1a",
  "phantom.command.list_item": "- {name} [{state}] \u884c\u6570={lines} \u7ef4\u5ea6={dimension} \u5750\u6807=({x}, {y}, {z})",
  "phantom.command.state_on": "\u542f\u7528",
  "phantom.command.state_off": "\u7981\u7528",

  "phantom.log.config_missing": "\u914d\u7f6e\u6587\u4ef6\u7f3a\u5931\u3001\u65e0\u6548\u6216\u5df2\u5347\u7ea7\uff1a{path}",
  "phantom.log.i18n_failed": "\u65e0\u6cd5\u51c6\u5907\u6216\u52a0\u8f7d Phantom \u8bed\u8a00\u6587\u4ef6",
  "phantom.log.store_missing": "\u60ac\u6d6e\u5b57\u5b58\u50a8\u6587\u4ef6\u7f3a\u5931\u6216\u65e0\u6548\uff1a{path}",
  "phantom.log.packet_validation_failed": "Sculk \u6570\u636e\u5305\u6821\u9a8c\u5931\u8d25\uff1a{packet} ({id})",
  "phantom.log.loading": "\u6b63\u5728\u52a0\u8f7d Phantom...",
  "phantom.log.enabling": "\u6b63\u5728\u542f\u7528 Phantom...",
  "phantom.log.disabling": "\u6b63\u5728\u7981\u7528 Phantom...",

  "phantom.form.main.title": "Phantom",
  "phantom.form.main.content": "\u60ac\u6d6e\u5b57\u7ba1\u7406\u5668",
  "phantom.form.main.create": "+ \u521b\u5efa",
  "phantom.form.main.reload": "\u91cd\u8f7d",
  "phantom.form.state.disabled": "\u5df2\u7981\u7528",
  "phantom.form.state.dynamic": "\u52a8\u6001",
  "phantom.form.state.static": "\u9759\u6001",
  "phantom.form.empty": "\u7a7a",
  "phantom.form.back": "< \u8fd4\u56de",

  "phantom.form.create.title": "\u521b\u5efa\u60ac\u6d6e\u5b57",
  "phantom.form.field.name": "\u540d\u79f0",
  "phantom.form.field.lines": "\u6587\u672c\u884c",
  "phantom.form.field.lines.placeholder": "\u6bcf\u884c\u4e00\u4e2a\u6587\u672c",
  "phantom.form.field.dimension": "\u7ef4\u5ea6 ID",
  "phantom.form.field.dimension.placeholder": "0\u30011\u30012 \u6216\u81ea\u5b9a\u4e49\u7ef4\u5ea6 ID",
  "phantom.form.field.dimension.custom": "\u652f\u6301\u81ea\u5b9a\u4e49\u7ef4\u5ea6",
  "phantom.form.field.enabled": "\u542f\u7528",

  "phantom.form.edit.title": "\u7f16\u8f91 {name}",
  "phantom.form.edit.content": "\u884c\u6570\uff1a{lines}\\n\u7ef4\u5ea6\uff1a{dimension}",
  "phantom.form.edit.lines": "\u7f16\u8f91\u6587\u672c\u884c",
  "phantom.form.edit.dynamic_lines": "\u52a8\u6001\u6587\u672c\u884c",
  "phantom.form.edit.move_to_me": "\u79fb\u52a8\u5230\u6211\u9644\u8fd1",
  "phantom.form.edit.enable": "\u542f\u7528",
  "phantom.form.edit.disable": "\u7981\u7528",
  "phantom.form.edit.advanced": "\u9ad8\u7ea7\u8bbe\u7f6e",
  "phantom.form.edit.delete": "\u5220\u9664",

  "phantom.form.advanced.title": "{name} \u7684\u9ad8\u7ea7\u8bbe\u7f6e",
  "phantom.form.field.view_distance": "\u53ef\u89c6\u8ddd\u79bb",
  "phantom.form.field.line_spacing": "\u884c\u8ddd",

  "phantom.form.lines.title": "{name} \u7684\u6587\u672c\u884c",

  "phantom.form.dynamic.title": "{name} \u7684\u52a8\u6001\u6587\u672c\u884c",
  "phantom.form.dynamic.content": "\u9009\u62e9\u4e00\u884c\u6765\u914d\u7f6e\u52a8\u6001\u5185\u5bb9\u3002",
  "phantom.form.dynamic.line_title": "\u52a8\u6001\u6587\u672c\u884c {line}",
  "phantom.form.dynamic.variables": "\u53d8\u91cf\uff1a{player}\u3001{online}\u3001{dimension}\u3001{hologram}\u3001{line}\u3001{contentIndex}\u3001{x}\u3001{y}\u3001{z}",
  "phantom.form.field.content_pool": "\u5185\u5bb9\u6c60",
  "phantom.form.field.content_pool.placeholder": "\u6bcf\u884c\u4e00\u4e2a\u5019\u9009\u5185\u5bb9",
  "phantom.form.field.update_interval": "\u66f4\u65b0\u95f4\u9694\uff08\u6beb\u79d2\uff09",
  "phantom.form.field.update_interval.placeholder": "0 = \u4e0d\u8f6e\u64ad",
  "phantom.form.field.parse_variables": "\u89e3\u6790\u53d8\u91cf",

  "phantom.form.delete.title": "\u5220\u9664 {name}",
  "phantom.form.delete.content": "\u8fd9\u4f1a\u4ece\u5b58\u50a8\u4e2d\u5220\u9664\u8be5\u60ac\u6d6e\u5b57\u3002",
  "phantom.form.delete.confirm": "\u5220\u9664",
  "phantom.form.delete.cancel": "\u53d6\u6d88"
}
)json";

bool ensureFile(std::filesystem::path const& path, std::string_view content) {
    if (std::filesystem::exists(path)) {
        return true;
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file{path, std::ios::binary};
    if (!file) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

} // namespace

bool setup(std::filesystem::path const& langDir) {
    auto ok = true;
    ok = ensureFile(langDir / "en_US.json", kEnUs) && ok;
    ok = ensureFile(langDir / "zh_CN.json", kZhCn) && ok;

    auto result = ll::i18n::getInstance().load(langDir);
    return ok && static_cast<bool>(result);
}

std::string normalizeLocale(std::string_view localeCode) {
    if (localeCode.starts_with("zh")) {
        return "zh_CN";
    }
    return "en_US";
}

std::string detectLeviLaminaLocale() {
    return normalizeLocale(ll::i18n::getDefaultLocaleCode());
}

std::string tr(std::string_view key, std::string_view localeCode) {
    auto& i18n  = ll::i18n::getInstance();
    auto  value = i18n.get(key, normalizeLocale(localeCode));
    if (value.empty()) {
        value = i18n.get(key, "en_US");
    }
    return std::string{value.empty() ? key : value};
}

} // namespace phantom::i18n
