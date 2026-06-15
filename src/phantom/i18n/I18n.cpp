#include "phantom/i18n/I18n.h"

#include "ll/api/i18n/I18n.h"

namespace phantom::i18n {

void setup() {
    auto& i18n = ll::i18n::getInstance();

    i18n.set("en_US", "phantom.command.player_only", "This command can only be used by a player.");
    i18n.set("en_US", "phantom.command.not_found", "Hologram not found.");
    i18n.set("en_US", "phantom.command.exists", "A hologram with that name already exists.");
    i18n.set("en_US", "phantom.command.created", "Hologram created.");
    i18n.set("en_US", "phantom.command.deleted", "Hologram deleted.");
    i18n.set("en_US", "phantom.command.updated", "Hologram updated.");
    i18n.set("en_US", "phantom.command.reloaded", "Holograms reloaded.");
    i18n.set("en_US", "phantom.command.empty", "No holograms have been created.");

    i18n.set("zh_CN", "phantom.command.player_only", "该命令只能由玩家执行。");
    i18n.set("zh_CN", "phantom.command.not_found", "找不到指定悬浮字。");
    i18n.set("zh_CN", "phantom.command.exists", "同名悬浮字已经存在。");
    i18n.set("zh_CN", "phantom.command.created", "悬浮字已创建。");
    i18n.set("zh_CN", "phantom.command.deleted", "悬浮字已删除。");
    i18n.set("zh_CN", "phantom.command.updated", "悬浮字已更新。");
    i18n.set("zh_CN", "phantom.command.reloaded", "悬浮字已重载。");
    i18n.set("zh_CN", "phantom.command.empty", "还没有创建任何悬浮字。");

    i18n.set("zh_TW", "phantom.command.player_only", "該命令只能由玩家執行。");
    i18n.set("zh_TW", "phantom.command.not_found", "找不到指定懸浮字。");
    i18n.set("zh_TW", "phantom.command.exists", "同名懸浮字已存在。");
    i18n.set("zh_TW", "phantom.command.created", "懸浮字已建立。");
    i18n.set("zh_TW", "phantom.command.deleted", "懸浮字已刪除。");
    i18n.set("zh_TW", "phantom.command.updated", "懸浮字已更新。");
    i18n.set("zh_TW", "phantom.command.reloaded", "懸浮字已重新載入。");
    i18n.set("zh_TW", "phantom.command.empty", "尚未建立任何懸浮字。");
}

std::string tr(std::string_view key, std::string_view localeCode) {
    auto& i18n  = ll::i18n::getInstance();
    auto  value = i18n.get(key, localeCode);
    if (value.empty()) {
        value = i18n.get(key, "en_US");
    }
    return std::string{value};
}

} // namespace phantom::i18n
