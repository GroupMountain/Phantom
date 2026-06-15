#include "mod/Phantom.h"

#include "phantom/commands/Commands.h"
#include "phantom/hologram/HologramService.h"
#include "phantom/i18n/I18n.h"

#include "ll/api/Config.h"
#include "ll/api/mod/RegisterHelper.h"

namespace phantom {
namespace {

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string logText(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args = {}) {
    auto text = i18n::tr(key, Phantom::getInstance().getLanguage());
    for (auto const& [name, value] : args) {
        std::string placeholder = "{";
        placeholder += name;
        placeholder += "}";
        replaceAll(text, placeholder, value);
    }
    return text;
}

} // namespace

Phantom& Phantom::getInstance() {
    static Phantom instance;
    return instance;
}

bool Phantom::loadConfig() {
    auto const configPath = getSelf().getConfigDir() / "config.json";
    auto       loaded     = ll::config::loadConfig(mConfig, configPath);
    if (mConfig.language.empty() || mConfig.language == "auto") {
        mConfig.language = i18n::detectLeviLaminaLocale();
    } else {
        mConfig.language = i18n::normalizeLocale(mConfig.language);
    }
    mLanguage = mConfig.language;

    if (!loaded) {
        getSelf().getLogger().warn("{}", logText("phantom.log.config_missing", {{"path", configPath.string()}}));
        ll::config::saveConfig(mConfig, configPath);
        return false;
    }
    ll::config::saveConfig(mConfig, configPath);
    return true;
}

bool Phantom::load() {
    getSelf().getLogger().debug("{}", logText("phantom.log.loading"));
    if (!i18n::setup(getSelf().getLangDir())) {
        getSelf().getLogger().warn("{}", logText("phantom.log.i18n_failed"));
    }
    loadConfig();
    return true;
}

bool Phantom::enable() {
    getSelf().getLogger().debug("{}", logText("phantom.log.enabling"));
    if (mConfig.holograms) {
        hologram::HologramService::getInstance().init();
    }
    commands::registerCommands();
    return true;
}

bool Phantom::disable() {
    getSelf().getLogger().debug("{}", logText("phantom.log.disabling"));
    hologram::HologramService::getInstance().shutdown();
    return true;
}

} // namespace phantom

LL_REGISTER_MOD(phantom::Phantom, phantom::Phantom::getInstance());
