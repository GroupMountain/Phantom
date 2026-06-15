#pragma once

#include "phantom/hologram/HologramTypes.h"

#include "ll/api/event/ListenerBase.h"

#include "mc/world/actor/player/Player.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace phantom::hologram {

class HologramService {
public:
    static HologramService& getInstance();

    void init();
    void shutdown();
    void tick();

    [[nodiscard]] std::vector<Hologram> list() const;
    [[nodiscard]] std::optional<Hologram> get(std::string const& name) const;

    bool create(Hologram hologram);
    bool remove(std::string const& name);
    bool rename(std::string const& oldName, std::string const& newName);
    bool setEnabled(std::string const& name, bool enabled);
    bool setPosition(std::string const& name, Vec3 const& position, int dimension);
    bool setOptions(std::string const& name, bool enabled, double viewDistance, double lineSpacing);
    bool setLines(std::string const& name, std::vector<std::string> lines);
    bool setLineDynamic(
        std::string const&       name,
        std::size_t              index,
        std::vector<std::string> content,
        uint64_t                 updateIntervalMs,
        bool                     parseVariables
    );
    bool appendLine(std::string const& name, std::string line);
    bool insertLine(std::string const& name, std::size_t index, std::string line);
    bool setLine(std::string const& name, std::size_t index, std::string line);
    bool removeLine(std::string const& name, std::size_t index);
    bool moveNearPlayer(std::string const& name, Player const& player);
    bool reload();
    bool save();

    void refreshPlayer(Player& player, bool force = false);
    void despawnAllFor(Player& player);
    void refreshAll(bool force = false);

private:
    HologramService() = default;

    [[nodiscard]] std::filesystem::path storePath() const;
    [[nodiscard]] Hologram* findUnlocked(std::string const& name);
    [[nodiscard]] Hologram const* findUnlocked(std::string const& name) const;

    void spawnLine(Player& player, Hologram const& hologram, std::size_t lineIndex, std::string const& text);
    void updateLine(Player& player, Hologram const& hologram, std::size_t lineIndex, std::string const& text);
    void moveLine(Player& player, Hologram const& hologram, std::size_t lineIndex);
    void removeLineFromClient(Player& player, std::string const& hologramName, std::size_t lineIndex);

    mutable std::mutex mMutex;
    HologramStore      mStore{};
    bool               mInitialized{false};
    int                mTickCounter{0};

    std::unordered_map<std::string, std::unordered_set<std::uint64_t>> mVisibleRuntimeIds;
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, std::pair<std::size_t, std::string>>>
        mLineContentCache;
    std::vector<ll::event::ListenerPtr>                               mListeners;
};

} // namespace phantom::hologram
