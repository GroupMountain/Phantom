#pragma once

#include "ll/api/mod/NativeMod.h"

namespace phantom {

class Phantom {
public:
    struct ModuleConfig {
        int    version{1};
        bool   holograms{true};
        bool   forms{true};
        double viewDistance{48.0};
        double lineSpacing{0.27};
        int    refreshIntervalTicks{10};
        int    dynamicRefreshIntervalTicks{10};
        std::string language{"auto"};
    };

    static Phantom& getInstance();

    Phantom() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    [[nodiscard]] ModuleConfig const& getConfig() const { return mConfig; }
    [[nodiscard]] std::string const& getLanguage() const { return mLanguage; }

    bool load();
    bool enable();
    bool disable();

private:
    bool loadConfig();

    ll::mod::NativeMod& mSelf;
    ModuleConfig        mConfig{};
    std::string         mLanguage{"en_US"};
};

} // namespace phantom
