#include "phantom/hologram/HologramService.h"

#include "mod/Phantom.h"
#include "phantom/i18n/I18n.h"
#include "phantom/net/DebugDrawerPacket.h"
#include "phantom/net/SculkPacket.h"

#include "ll/api/Config.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/world/level/Level.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>
#include <utility>

namespace phantom::hologram {
namespace {

auto& logger() { return Phantom::getInstance().getSelf().getLogger(); }

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
        std::string placeholder  = "{";
        placeholder             += name;
        placeholder             += "}";
        replaceAll(text, placeholder, value);
    }
    return text;
}

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

[[nodiscard]] sculk::protocol::Vec3 toProtocol(Vec3 const& pos) { return {pos.x, pos.y, pos.z}; }

[[nodiscard]] uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}

[[nodiscard]] std::vector<std::string> contentPool(HologramLine const& line) {
    if (!line.content.empty()) {
        return line.content;
    }
    return {line.text};
}

[[nodiscard]] std::size_t currentContentIndex(HologramLine const& line) {
    auto const pool = contentPool(line);
    if (pool.size() <= 1 || line.updateIntervalMs == 0) {
        return 0;
    }
    return static_cast<std::size_t>((nowMs() / line.updateIntervalMs) % pool.size());
}

[[nodiscard]] std::string renderVariables(
    std::string     value,
    Player&         player,
    Hologram const& hologram,
    std::size_t     lineIndex,
    std::size_t     contentIndex
) {
    auto const pos = player.getPosition();
    replaceAll(value, "{player}", player.getRealName());
    replaceAll(value, "{dimension}", std::to_string(dimOf(player)));
    replaceAll(value, "{hologram}", hologram.name);
    replaceAll(value, "{line}", std::to_string(lineIndex + 1));
    replaceAll(value, "{contentIndex}", std::to_string(contentIndex));
    replaceAll(value, "{x}", std::to_string(static_cast<int>(std::floor(pos.x))));
    replaceAll(value, "{y}", std::to_string(static_cast<int>(std::floor(pos.y))));
    replaceAll(value, "{z}", std::to_string(static_cast<int>(std::floor(pos.z))));
    if (auto level = ll::service::getLevel()) {
        int online = 0;
        level->forEachPlayer([&](Player&) {
            ++online;
            return true;
        });
        replaceAll(value, "{online}", std::to_string(online));
    }
    return value;
}

[[nodiscard]] std::vector<std::string> resolveBaseLines(Player& player, Hologram const& hologram) {
    std::vector<std::string> lines;
    lines.reserve(hologram.lines.size());
    for (std::size_t i = 0; i < hologram.lines.size(); ++i) {
        auto const& line  = hologram.lines[i];
        auto const  pool  = contentPool(line);
        auto const  index = std::min(currentContentIndex(line), pool.size() - 1);
        auto        text  = pool[index];
        if (line.parseVariables) {
            text = renderVariables(std::move(text), player, hologram, i, index);
        }
        lines.emplace_back(std::move(text));
    }
    return lines;
}

[[nodiscard]] std::string joinLines(std::vector<std::string> const& lines) {
    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            text.push_back('\n');
        }
        text += lines[i];
    }
    return text;
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
    mListeners.emplace_back(
        eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>([](ll::event::player::PlayerJoinEvent& event) {
            HologramService::getInstance().refreshPlayer(event.self(), true);
        })
    );
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
    mVisibleShapeIds.clear();
    mLineContentCache.clear();
    mLineCallbacks.clear();
    mLineCallbackUpdateCache.clear();
    mInitialized = false;
}

void HologramService::tick() {
    if (!mInitialized) {
        return;
    }
    auto const interval = std::min(
        std::max(1, Phantom::getInstance().getConfig().refreshIntervalTicks),
        std::max(1, Phantom::getInstance().getConfig().dynamicRefreshIntervalTicks)
    );
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
    auto const*      hologram = findUnlocked(normalizeName(name));
    if (hologram == nullptr) {
        return std::nullopt;
    }
    return *hologram;
}

Hologram* HologramService::findUnlocked(std::string const& name) {
    auto const normalized = normalizeName(name);
    auto       iter =
        std::ranges::find_if(mStore.holograms, [&](Hologram const& hologram) { return hologram.name == normalized; });
    return iter == mStore.holograms.end() ? nullptr : &*iter;
}

Hologram const* HologramService::findUnlocked(std::string const& name) const {
    auto const normalized = normalizeName(name);
    auto       iter =
        std::ranges::find_if(mStore.holograms, [&](Hologram const& hologram) { return hologram.name == normalized; });
    return iter == mStore.holograms.end() ? nullptr : &*iter;
}

bool HologramService::create(Hologram hologram) {
    hologram.name = normalizeName(hologram.name);
    if (hologram.lines.empty()) {
        hologram.lines.push_back({"New hologram"});
    }
    for (auto& line : hologram.lines) {
        if (line.content.empty()) {
            line.content.push_back(line.text);
        }
        line.text = line.content.front();
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
        auto             iter = std::ranges::remove_if(mStore.holograms, [&](Hologram const& hologram) {
            return hologram.name == normalized;
        });
        if (iter.begin() == mStore.holograms.end()) {
            return false;
        }
        mStore.holograms.erase(iter.begin(), iter.end());
        mLineCallbacks.erase(normalized);
        mLineCallbackUpdateCache.clear();
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
        if (auto callbackIter = mLineCallbacks.find(normalizedOld); callbackIter != mLineCallbacks.end()) {
            mLineCallbacks[normalizedNew] = std::move(callbackIter->second);
            mLineCallbacks.erase(callbackIter);
        }
        mLineCallbackUpdateCache.clear();
        hologram->name = normalizedNew;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setEnabled(std::string const& name, bool enabled) {
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
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
        auto*            hologram = findUnlocked(name);
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
        auto*            hologram = findUnlocked(name);
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
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->lines.clear();
        for (auto& line : lines) {
            hologram->lines.push_back({std::move(line)});
            hologram->lines.back().content.push_back(hologram->lines.back().text);
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
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr) {
            return false;
        }
        hologram->lines.push_back({std::move(line)});
        hologram->lines.back().content.push_back(hologram->lines.back().text);
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::insertLine(std::string const& name, std::size_t index, std::string line) {
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr || index > hologram->lines.size()) {
            return false;
        }
        HologramLine next{std::move(line)};
        next.content.push_back(next.text);
        hologram->lines.insert(hologram->lines.begin() + static_cast<std::ptrdiff_t>(index), std::move(next));
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setLine(std::string const& name, std::size_t index, std::string line) {
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        hologram->lines[index].text             = std::move(line);
        hologram->lines[index].content          = {hologram->lines[index].text};
        hologram->lines[index].updateIntervalMs = 0;
        hologram->lines[index].parseVariables   = false;
        if (auto callbackIter = mLineCallbacks.find(hologram->name); callbackIter != mLineCallbacks.end()) {
            callbackIter->second.erase(index);
            if (callbackIter->second.empty()) {
                mLineCallbacks.erase(callbackIter);
            }
        }
        mLineCallbackUpdateCache.clear();
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setLineDynamic(
    std::string const&       name,
    std::size_t              index,
    std::vector<std::string> content,
    uint64_t                 updateIntervalMs,
    bool                     parseVariables
) {
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        if (content.empty()) {
            content.push_back("");
        }
        hologram->lines[index].content          = std::move(content);
        hologram->lines[index].text             = hologram->lines[index].content.front();
        hologram->lines[index].updateIntervalMs = updateIntervalMs;
        hologram->lines[index].parseVariables   = parseVariables;
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::setLineCallback(
    std::string const&       name,
    std::size_t              index,
    HologramLineTextCallback callback,
    uint64_t                 updateIntervalMs
) {
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        if (callback) {
            mLineCallbacks[hologram->name][index] = {
                .callback         = std::move(callback),
                .updateIntervalMs = updateIntervalMs,
            };
        } else if (auto callbackIter = mLineCallbacks.find(hologram->name); callbackIter != mLineCallbacks.end()) {
            callbackIter->second.erase(index);
            if (callbackIter->second.empty()) {
                mLineCallbacks.erase(callbackIter);
            }
        }
        mLineCallbackUpdateCache.clear();
    }
    refreshAll(true);
    return true;
}

bool HologramService::clearLineCallback(std::string const& name, std::size_t index) {
    return setLineCallback(name, index, {});
}

bool HologramService::removeLine(std::string const& name, std::size_t index) {
    refreshAll(true);
    {
        std::scoped_lock lock{mMutex};
        auto*            hologram = findUnlocked(name);
        if (hologram == nullptr || index >= hologram->lines.size()) {
            return false;
        }
        hologram->lines.erase(hologram->lines.begin() + static_cast<std::ptrdiff_t>(index));
        if (auto callbackIter = mLineCallbacks.find(hologram->name); callbackIter != mLineCallbacks.end()) {
            std::unordered_map<std::size_t, HologramLineCallbackEntry> nextCallbacks;
            for (auto& [lineIndex, callback] : callbackIter->second) {
                if (lineIndex < index) {
                    nextCallbacks.emplace(lineIndex, std::move(callback));
                } else if (lineIndex > index) {
                    nextCallbacks.emplace(lineIndex - 1, std::move(callback));
                }
            }
            if (nextCallbacks.empty()) {
                mLineCallbacks.erase(callbackIter);
            } else {
                callbackIter->second = std::move(nextCallbacks);
            }
        }
        mLineCallbackUpdateCache.clear();
        if (hologram->lines.empty()) {
            hologram->lines.push_back({""});
            hologram->lines.back().content.push_back(hologram->lines.back().text);
        }
    }
    save();
    refreshAll(true);
    return true;
}

bool HologramService::moveNearPlayer(std::string const& name, Player const& player) {
    auto pos  = player.getPosition();
    pos.y    += 2.2f;
    return setPosition(name, pos, dimOf(player));
}

bool HologramService::reload() {
    refreshAll(true);
    HologramStore next;
    auto const    path = storePath();
    if (!ll::config::loadConfig(next, path)) {
        logger().warn(
            "{}",
            logText(
                "phantom.log.store_missing",
                {
                    {"path", path.string()}
        }
            )
        );
        ll::config::saveConfig(next, path);
    }
    for (auto& hologram : next.holograms) {
        hologram.name = normalizeName(hologram.name);
        if (hologram.lines.empty()) {
            hologram.lines.push_back({""});
        }
        for (auto& line : hologram.lines) {
            if (line.content.empty()) {
                line.content.push_back(line.text);
            }
            line.text = line.content.front();
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

void HologramService::sendHologram(Player& player, Hologram const& hologram, std::string const& text) {
    net::DebugDrawerPacket packet;
    packet.shapes.push_back({
        .networkId   = runtimeIdFor(hologram.name, 0),
        .location    = toProtocol(hologram.position),
        .dimensionId = hologram.dimension,
        .text        = text,
        .remove      = false,
    });
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::removeHologramFromClient(Player& player, Hologram const& hologram) {
    net::DebugDrawerPacket packet;
    packet.shapes.push_back({
        .networkId   = runtimeIdFor(hologram.name, 0),
        .dimensionId = hologram.dimension,
        .remove      = true,
    });
    net::sendSculkPacketTo(player, packet, logger());
}

void HologramService::refreshPlayer(Player& player, bool force) {
    std::vector<Hologram> snapshot  = list();
    auto const            playerKey = uuidOf(player);

    std::unordered_set<std::uint64_t> expected;
    for (auto const& hologram : snapshot) {
        if (!nearEnough(player, hologram)) {
            continue;
        }
        expected.insert(runtimeIdFor(hologram.name, 0));
    }

    std::unordered_set<std::uint64_t> previous;
    {
        std::scoped_lock lock{mMutex};
        previous = mVisibleShapeIds[playerKey];
    }

    if (force) {
        for (auto runtimeId : previous) {
            for (auto const& hologram : snapshot) {
                if (runtimeId == runtimeIdFor(hologram.name, 0)) {
                    removeHologramFromClient(player, hologram);
                }
            }
        }
        previous.clear();
        std::scoped_lock lock{mMutex};
        mLineContentCache[playerKey].clear();
    }

    for (auto const& hologram : snapshot) {
        auto const runtimeId = runtimeIdFor(hologram.name, 0);
        if (!expected.contains(runtimeId)) {
            if (previous.contains(runtimeId)) {
                removeHologramFromClient(player, hologram);
                std::scoped_lock lock{mMutex};
                mLineContentCache[playerKey].erase(runtimeId);
            }
            continue;
        }

        std::optional<std::string> resolvedText;
        std::unordered_map<std::size_t, HologramLineCallbackEntry> callbacks;
        {
            std::scoped_lock lock{mMutex};
            if (auto callbackIter = mLineCallbacks.find(hologram.name); callbackIter != mLineCallbacks.end()) {
                callbacks = callbackIter->second;
            }
        }

        auto resolvedLines = resolveBaseLines(player, hologram);
        if (!callbacks.empty()) {
            bool shouldInvoke = force;
            {
                std::scoped_lock lock{mMutex};
                auto&    callbackCache = mLineCallbackUpdateCache[playerKey][runtimeId];
                uint64_t nextDue       = callbackCache;
                if (!shouldInvoke) {
                    for (auto const& [lineIndex, entry] : callbacks) {
                        if (!entry.callback) {
                            continue;
                        }
                        if (entry.updateIntervalMs == 0 || nextDue == 0 || nowMs() >= nextDue) {
                            shouldInvoke = true;
                            break;
                        }
                    }
                }
            }
            if (shouldInvoke) {
                for (auto const& [lineIndex, entry] : callbacks) {
                    if (!entry.callback) {
                        continue;
                    }
                    entry.callback(player, resolvedLines);
                }
                uint64_t earliestNextDue = 0;
                auto     currentNow      = nowMs();
                for (auto const& [lineIndex, entry] : callbacks) {
                    if (entry.updateIntervalMs == 0) {
                        continue;
                    }
                    auto due = currentNow + entry.updateIntervalMs;
                    if (earliestNextDue == 0 || due < earliestNextDue) {
                        earliestNextDue = due;
                    }
                }
                std::scoped_lock lock{mMutex};
                mLineCallbackUpdateCache[playerKey][runtimeId] = earliestNextDue;
            }
        }

        if (!resolvedLines.empty()) {
            resolvedText = joinLines(resolvedLines);
        }

        if (!resolvedText) {
            if (previous.contains(runtimeId)) {
                removeHologramFromClient(player, hologram);
                std::scoped_lock lock{mMutex};
                mLineContentCache[playerKey].erase(runtimeId);
            }
            continue;
        }

        if (!previous.contains(runtimeId)) {
            sendHologram(player, hologram, *resolvedText);
            std::scoped_lock lock{mMutex};
            mLineContentCache[playerKey][runtimeId] = std::move(*resolvedText);
        } else {
            bool shouldUpdate = force;
            {
                std::scoped_lock lock{mMutex};
                auto&            cache = mLineContentCache[playerKey][runtimeId];
                if (cache != *resolvedText) {
                    shouldUpdate = true;
                    cache        = *resolvedText;
                }
            }
            if (shouldUpdate) {
                sendHologram(player, hologram, *resolvedText);
            }
        }
    }

    {
        std::scoped_lock lock{mMutex};
        mVisibleShapeIds[playerKey] = std::move(expected);
    }
}

void HologramService::despawnAllFor(Player& player) {
    auto const playerKey = uuidOf(player);
    {
        std::scoped_lock lock{mMutex};
        mVisibleShapeIds.erase(playerKey);
        mLineContentCache.erase(playerKey);
        mLineCallbackUpdateCache.erase(playerKey);
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
