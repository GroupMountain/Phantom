#include "mod/Phantom.h"

#include "phantom/commands/Commands.h"
#include "phantom/hologram/HologramService.h"
#include "phantom/i18n/I18n.h"

#include "ll/api/Config.h"
#include "ll/api/mod/RegisterHelper.h"

namespace phantom {

Phantom& Phantom::getInstance() {
    static Phantom instance;
    return instance;
}

bool Phantom::loadConfig() {
    auto const configPath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(mConfig, configPath)) {
        getSelf().getLogger().warn("Config file was missing, invalid, or upgraded: {}", configPath.string());
        ll::config::saveConfig(mConfig, configPath);
        return false;
    }
    return true;
}

bool Phantom::load() {
    getSelf().getLogger().debug("Loading Phantom...");
    loadConfig();
    i18n::setup();
    return true;
}

bool Phantom::enable() {
    getSelf().getLogger().debug("Enabling Phantom...");
    if (mConfig.holograms) {
        hologram::HologramService::getInstance().init();
    }
    commands::registerCommands();
    return true;
}

bool Phantom::disable() {
    getSelf().getLogger().debug("Disabling Phantom...");
    hologram::HologramService::getInstance().shutdown();
    return true;
}

} // namespace phantom

LL_REGISTER_MOD(phantom::Phantom, phantom::Phantom::getInstance());
