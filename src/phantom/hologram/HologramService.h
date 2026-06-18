#pragma once

#include "phantom/hologram/HologramTypes.h"
#include "phantom/net/DebugDrawerPacket.h"

#include "ll/api/event/ListenerBase.h"

#include "mc/network/NetworkIdentifier.h"
#include "mc/world/actor/player/Player.h"

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace phantom::hologram {

using HologramLineTextCallback = std::function<void(Player& player, std::vector<std::string>& currentLines)>;

struct HologramLineCallbackEntry {
    HologramLineTextCallback callback;
    uint64_t                 updateIntervalMs{0};
};

struct PendingHologramPacket {
    net::DebugDrawerPacket::Shape shape;
};

class HologramService {
public:
    static HologramService& getInstance();

    void init();
    void shutdown();
    void tick();

    [[nodiscard]] std::vector<Hologram>   list() const;
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
    bool setLineCallback(
        std::string const&       name,
        std::size_t              index,
        HologramLineTextCallback callback,
        uint64_t                 updateIntervalMs = 0
    );
    bool clearLineCallback(std::string const& name, std::size_t index);
    bool appendLine(std::string const& name, std::string line);
    bool insertLine(std::string const& name, std::size_t index, std::string line);
    bool setLine(std::string const& name, std::size_t index, std::string line);
    bool removeLine(std::string const& name, std::size_t index);
    bool moveNearPlayer(std::string const& name, Player const& player);
    bool reload();
    bool save();

    void refreshPlayer(Player& player, bool force = false);
    void refreshHologram(Hologram const& hologram, bool force = false);
    void despawnAllFor(Player& player);
    void markPlayerInitialized(NetworkIdentifier const& networkId);
    [[nodiscard]] bool isPlayerInitialized(Player const& player) const;
    void flushPendingPackets(Player& player);
    void refreshAll(bool force = false);

private:
    HologramService() = default;

    [[nodiscard]] std::filesystem::path storePath() const;
    [[nodiscard]] Hologram*             findUnlocked(std::string const& name);
    [[nodiscard]] Hologram const*       findUnlocked(std::string const& name) const;

    void sendHologram(Player& player, Hologram const& hologram, std::string const& text);
    void removeHologramFromClient(Player& player, Hologram const& hologram);
    void queuePendingShape(Player& player, net::DebugDrawerPacket::Shape shape);

    mutable std::mutex mMutex;
    HologramStore      mStore{};
    bool               mInitialized{false};
    int                mTickCounter{0};

    std::unordered_map<std::string, std::unordered_set<std::uint64_t>>                          mVisibleShapeIds;
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, std::string>>             mLineContentCache;
    std::unordered_map<std::string, std::unordered_map<std::size_t, HologramLineCallbackEntry>> mLineCallbacks;
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, uint64_t>>                mLineCallbackUpdateCache;
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, PendingHologramPacket>>   mPendingPackets;
    std::unordered_set<std::string>                                                              mInitializedPlayers;
    std::vector<ll::event::ListenerPtr>                                                          mListeners;
};

} // namespace phantom::hologram
