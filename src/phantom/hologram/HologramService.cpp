#include "phantom/hologram/HologramService.h"

#include "mod/Phantom.h"
#include "phantom/net/SculkPacket.h"

#include "ll/api/Config.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/world/level/Level.h"

#include <sculk/protocol/codec/actor/ActorDataIDs.hpp>
#include <sculk/protocol/codec/packet/AddActorPacket.hpp>
#include <sculk/protocol/codec/packet/MoveActorAbsolutePacket.hpp>
#include <sculk/protocol/codec/packet/RemoveActorPacket.hpp>
#include <sculk/protocol/codec/packet/SetActorDataPacket.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace phantom::hologram {
namespace {

auto& logger() { return Phantom::getInstance().getSelf().getLogger(); }

[[nodiscard]] std::string uuidOf(Player const& player) { return player.getUuid().asString(); }

[[nodiscard]] int dimOf(Player const& player) { return static_cast<int>(player.getDimensionId()); }

[[nodiscard]] double configuredViewDistance(Hologram const& hologram) {
    return hologram.viewDistance > 0.0 ? hologram.viewDistance : Phantom::getInstance().getConfig().viewDistance;
}

[[nodiscard]] double configuredLineSpacing(Hologram const& hologram) {
    return hologram.lineSpacing > 0.0 ? hologram.lineSpacing : Phantom::getInstance().getConfig().lineSpacing;
}

[[nodiscard]] bool nearEnough(Player const& player, Hologram const& hologram) {
    if (!hologram.enabled || dimOf(player) != hologram.dimension) {
        return false;
    }
    auto const distance = configuredViewDistance(hologram);
    auto const pp       = player.getPosition();
    auto const hp       = hologram.position;
    auto const dx       = static_cast<double>(pp.x - hp.x);
    auto const dy       = static_cast<double>(pp.y - hp.y);
    auto const dz       = static_cast<double>(pp.z - hp.z);
    return dx * dx + dy * dy + dz * dz <= distance * distance;
}

[[nodiscard]] sculk::protocol::Vec3 toProtocol(Vec3 const& pos) {
    return {pos.x, pos.y, pos.z};
}

[[nodiscard]] Vec3 linePosition(Hologram const& hologram, std::size_t lineIndex) {
    auto pos = hologram.position;
    pos.y -= static_cast<float>(configuredLineSpacing(hologram) * static_cast<double>(lineIndex));
    return pos;
}

[[nodiscard]] std::string displayText(Hologram const& hologram, std::size_t lineIndex) {
    if (lineIndex >= hologram.lines.size()) {
        return {};
    }
    return hologram.lines[lineIndex].text;
}

[[nodiscard]] int64_t invisibleNametagFlags() {
    return (int64_t{1} << 5) | (int64_t{1} << 14) | (int64_t{1} << 15);
}

LL_AUTO_TYPE_INSTANCE_HOOK(PhantomLevelTickHook, ll::memory::HookPriority::Normal, Level, &Level::$tick, void) {
    origin();
    HologramService::getInstance().tick();
}

} // namespace

HologramService& HologramService::getInstance() {
    static HologramService instance;
    return instance;
}

std::filesystem::path HologramService::storePath() const {
    return Phantom::getInstance().getSelf().getDataDir() / "holograms.json";
}

void HologramService::init() {
    if (mInitialized) {
        return;
    }
    reload();
    auto& eventBus = ll::event::EventBus::getInstance();
    mListeners.emplace_back(eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>(
        [](ll::event::player::PlayerJoinEvent& event) {
            HologramService::getInstance().refreshPlayer(event.self(), true);
        }
    ));
    mListeners.emplace_back(eventBus.emplaceListener<ll::event::player::PlayerDisconnectEvent>(
        [](ll::event::player::PlayerDisconnectEvent& event) {
            HologramService::getInstance().despawnAllFor(event.self());
        }
    ));
    mInitialized = true;
}

void HologramService::shutdown() {
    refreshAll(true);
    auto& eventBus = ll::event::EventBus::getInstance();
    for (auto const& listener : mListeners) {
        if (listener) {
            eventBus.removeListener(listener);
        }
    }
    mListeners.clear();
    std::scoped_lock lock{mMutex};
    mVisibleRuntimeIds.clear();
    mInitialized = false;
}

void HologramService::tick() {
    if (!mInitialized) {
        return;
    }
    auto const interval = std::max(1, Phantom::getInstance().getConfig().refreshIntervalTicks);
    if (++mTickCounter % interval != 0) {
        return;
    }
    refreshAll(false);
}

std::vector<Hologram> HologramService::list() const {
    std::scoped_lock lock{mMutex};
    return mStore.holograms;
}

std::optional<Hologram> HologramService::get(std::string const& name) const {
    std::scoped_lock lock{mMutex};
    auto const* hologram = findUnlocked(normalizeName(name));
    if (hologram == nullptr) {
        return std::nullopt;
    }
    return *hologram;
}

Hologram* HologramService::findUnlocked(std::string const& name) {
    auto const normalized = normalizeName(name);
    auto iter = std::ranges::find_if(mStore.holograms, [&](Hologram const& hologram) {
        return hologram.name == normalized;
    });
    return iter == mStore.holograms.end() ? nullptr : &*iter;
}

Hologram const* HologramService::findUnlocked(std::string const& name) const {
    auto const normalized = normalizeName(name);
    auto iter = std::ranges::find_if(mStore.holograms, [&](Hologram const& hologram) {
        return hologram.name == normalized;
    });
    return iter == mStore.holograms.end() ? nullptr : &*iter;
}

bool HologramService::create(Hologram hologram) {
    hologram.name = normalizeName(hologram.name);
    if (hologram.lines.empty()) {
        hologram.lines.push_back({"New hologram"});
    }
    {
        std::scoped_lock lock{mMutex};
        if (findUnlocked(hologram.name) != nullptr) {
            return false;
        }
        mStore.holograms.push_back(std::move(hologram));
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::remove(std::string const& name) {
    auto normalized = normalizeName(name);
    refreshAll(true);
    {
        std::scoped_lock lock{mMutex};
        auto iter = std::ranges::remove_if(mStore.holograms, [&](Hologram const& hologram) {
            return hologram.name == normalized;
        });
        if (iter.begin() == mStore.holograms.end()) {
            return false;
        }
        mStore.holograms.erase(iter.begin(), iter.end());
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::rename(std::string const& oldName, std::string const& newName) {
    auto normalizedOld = normalizeName(oldName);
    auto normalizedNew = normalizeName(newName);
    refreshAll(true);
    {
        std::scoped_lock lock{mMutex};
        if (findUnlocked(normalizedNew) != nullptr) {
            return false;
        }
        auto* hologram = findUnlocked(normalizedOld);
        if (hologram == nullptr) {
            return false;
        }
        hologram->name = normalizedNew;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setEnabled(std::string const& name, bool enabled) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->enabled = enabled;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setPosition(std::string const& name, Vec3 const& position, int dimension) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->position  = position;
        hologram->dimension = dimension;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setOptions(std::string const& name, bool enabled, double viewDistance, double lineSpacing) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->enabled      = enabled;
        hologram->viewDistance = viewDistance;
        hologram->lineSpacing  = lineSpacing;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setLines(std::string const& name, std::vector<std::string> lines) {
    refreshAll(true);
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->lines.clear();
        for (auto& line : lines) {
            hologram->lines.push_back({std::move(line)});
        }
        if (hologram->lines.empty()) {
            hologram->lines.push_back({""});
        }
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::appendLine(std::string const& name, std::string line) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->lines.push_back({std::move(line)});
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::insertLine(std::string const& name, std::size_t index, std::string line) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr || index > hologram->lines.size()) {
            return false;
        }
        hologram->lines.insert(hologram->lines.begin() + static_cast<std::ptrdiff_t>(index), {std::move(line)});
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setLine(std::string const& name, std::size_t index, std::string line) {
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        hologram->lines[index].text = std::move(line);
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::removeLine(std::string const& name, std::size_t index) {
    refreshAll(true);
    {
        std::scoped_lock lock{mMutex};
        auto* hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        hologram->lines.erase(hologram->lines.begin() + static_cast<std::ptrdiff_t>(index));
        if (hologram->lines.empty()) {
            hologram->lines.push_back({""});
        }
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::moveNearPlayer(std::string const& name, Player const& player) {
    auto pos = player.getPosition();
    pos.y += 2.2f;
    return setPosition(name, pos, dimOf(player));
}

bool HologramService::reload() {
    refreshAll(true);
    HologramStore next;
    auto const    path = storePath();
    if (!ll::config::loadConfig(next, path)) {
        logger().warn("Hologram store was missing or invalid: {}", path.string());
        ll::config::saveConfig(next, path);
    }
    for (auto& hologram : next.holograms) {
        hologram.name = normalizeName(hologram.name);
        if (hologram.lines.empty()) {
            hologram.lines.push_back({""});
        }
    }
    {
        std::scoped_lock lock{mMutex};
        mStore = std::move(next);
    }
    refreshAll(true);
    return true;
}

bool HologramService::save() {
    HologramStore snapshot;
    {
        std::scoped_lock lock{mMutex};
        snapshot = mStore;
    }
    return ll::config::saveConfig(snapshot, storePath());
}

void HologramService::spawnLine(Player& player, Hologram const& hologram, std::size_t lineIndex) {
    sculk::protocol::AddActorPacket packet;
    packet.mActorRuntimeId = runtimeIdFor(hologram.name, lineIndex);
    packet.mActorUniqueId  = uniqueIdFor(hologram.name, lineIndex);
    packet.mIdentifier     = "minecraft:armor_stand";
    packet.mPosition       = toProtocol(linePosition(hologram, lineIndex));
    packet.mVelocity       = {0.0f, 0.0f, 0.0f};
    packet.mRotation       = {0.0f, 0.0f};
    packet.mYHeadRotation  = 0.0f;
    packet.mYBodyRotation  = 0.0f;
    packet.mMetaData.mDataItems = {
        {sculk::protocol::ActorDataIDs::Reserved0, invisibleNametagFlags()},
        {sculk::protocol::ActorDataIDs::Name, displayText(hologram, lineIndex)},
        {sculk::protocol::ActorDataIDs::NametagAlwaysShow, static_cast<std::uint8_t>(1)},
        {sculk::protocol::ActorDataIDs::NameplateRenderDistanceMax, static_cast<float>(configuredViewDistance(hologram))}
    };
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::updateLine(Player& player, Hologram const& hologram, std::size_t lineIndex) {
    sculk::protocol::SetActorDataPacket packet;
    packet.mActorRuntimeId = runtimeIdFor(hologram.name, lineIndex);
    packet.mTick           = 0;
    packet.mMetaData.mDataItems = {
        {sculk::protocol::ActorDataIDs::Reserved0, invisibleNametagFlags()},
        {sculk::protocol::ActorDataIDs::Name, displayText(hologram, lineIndex)},
        {sculk::protocol::ActorDataIDs::NametagAlwaysShow, static_cast<std::uint8_t>(1)},
        {sculk::protocol::ActorDataIDs::NameplateRenderDistanceMax, static_cast<float>(configuredViewDistance(hologram))}
    };
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::moveLine(Player& player, Hologram const& hologram, std::size_t lineIndex) {
    sculk::protocol::MoveActorAbsolutePacket packet;
    packet.mActorRuntimeId  = runtimeIdFor(hologram.name, lineIndex);
    packet.mHeader          = 0;
    packet.mPosition        = toProtocol(linePosition(hologram, lineIndex));
    packet.mRotationX       = 0;
    packet.mRotationY       = 0;
    packet.mRotationYHead   = 0;
    packet.mForceCompletion = true;
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::removeLineFromClient(Player& player, std::string const& hologramName, std::size_t lineIndex) {
    sculk::protocol::RemoveActorPacket packet;
    packet.mActorUniqueId = uniqueIdFor(hologramName, lineIndex);
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::refreshPlayer(Player& player, bool force) {
    std::vector<Hologram> snapshot = list();
    auto const            playerKey = uuidOf(player);

    std::unordered_set<std::uint64_t> expected;
    for (auto const& hologram : snapshot) {
        if (!nearEnough(player, hologram)) {
            continue;
        }
        for (std::size_t i = 0; i < hologram.lines.size(); ++i) {
            expected.insert(runtimeIdFor(hologram.name, i));
        }
    }

    std::unordered_set<std::uint64_t> previous;
    {
        std::scoped_lock lock{mMutex};
        previous = mVisibleRuntimeIds[playerKey];
    }

    if (force) {
        for (auto runtimeId : previous) {
            for (auto const& hologram : snapshot) {
                for (std::size_t i = 0; i < hologram.lines.size(); ++i) {
                    if (runtimeId == runtimeIdFor(hologram.name, i)) {
                        removeLineFromClient(player, hologram.name, i);
                    }
                }
            }
        }
        previous.clear();
    }

    for (auto const& hologram : snapshot) {
        for (std::size_t i = 0; i < hologram.lines.size(); ++i) {
            auto const runtimeId = runtimeIdFor(hologram.name, i);
            if (!expected.contains(runtimeId)) {
                if (previous.contains(runtimeId)) {
                    removeLineFromClient(player, hologram.name, i);
                }
                continue;
            }
            if (!previous.contains(runtimeId)) {
                spawnLine(player, hologram, i);
            } else {
                moveLine(player, hologram, i);
                updateLine(player, hologram, i);
            }
        }
    }

    {
        std::scoped_lock lock{mMutex};
        mVisibleRuntimeIds[playerKey] = std::move(expected);
    }
}

void HologramService::despawnAllFor(Player& player) {
    auto const playerKey = uuidOf(player);
    {
        std::scoped_lock lock{mMutex};
        mVisibleRuntimeIds.erase(playerKey);
    }
}

void HologramService::refreshAll(bool force) {
    auto level = ll::service::getLevel();
    if (!level) {
        return;
    }
    level->forEachPlayer([&](Player& player) {
        refreshPlayer(player, force);
        return true;
    });
}

} // namespace phantom::hologram
