#include "phantom/service/PhantomServiceRegistrar.h"

#include "phantom/hologram/HologramService.h"
#include "phantom/service/PhantomHologramService.h"

#include "ll/api/service/ServiceManager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace phantom::service {
namespace {

[[nodiscard]] PhantomHologramLine toExternalLine(hologram::HologramLine const& line) {
    return PhantomHologramLine{
        .text             = line.text,
        .content          = line.content,
        .updateIntervalMs = line.updateIntervalMs,
        .parseVariables   = line.parseVariables,
    };
}

[[nodiscard]] PhantomHologram toExternal(hologram::Hologram const& hologram) {
    std::vector<PhantomHologramLine> lines;
    lines.reserve(hologram.lines.size());
    for (auto const& line : hologram.lines) {
        lines.emplace_back(toExternalLine(line));
    }

    return PhantomHologram{
        .name         = hologram.name,
        .dimension    = hologram.dimension,
        .position     = hologram.position,
        .lines        = std::move(lines),
        .enabled      = hologram.enabled,
        .viewDistance = hologram.viewDistance,
        .lineSpacing  = hologram.lineSpacing,
        .persistent   = hologram.persistent,
    };
}

[[nodiscard]] hologram::HologramLine toInternalLine(PhantomHologramLine const& line) {
    return hologram::HologramLine{
        .text             = line.text,
        .content          = line.content,
        .updateIntervalMs = line.updateIntervalMs,
        .parseVariables   = line.parseVariables,
    };
}

[[nodiscard]] hologram::Hologram toInternal(PhantomHologram const& hologram) {
    std::vector<hologram::HologramLine> lines;
    lines.reserve(hologram.lines.size());
    for (auto const& line : hologram.lines) {
        lines.emplace_back(toInternalLine(line));
    }

    return hologram::Hologram{
        .name         = hologram.name,
        .dimension    = hologram.dimension,
        .position     = hologram.position,
        .lines        = std::move(lines),
        .enabled      = hologram.enabled,
        .viewDistance = hologram.viewDistance,
        .lineSpacing  = hologram.lineSpacing,
        .persistent   = hologram.persistent,
    };
}

class PhantomHologramServiceImpl final : public PhantomHologramService {
public:
    std::vector<PhantomHologram> list() const override {
        auto                         internal = hologram::HologramService::getInstance().list();
        std::vector<PhantomHologram> result;
        result.reserve(internal.size());
        for (auto const& entry : internal) {
            result.emplace_back(toExternal(entry));
        }
        return result;
    }

    std::optional<PhantomHologram> get(std::string const& name) const override {
        auto result = hologram::HologramService::getInstance().get(name);
        if (!result) {
            return std::nullopt;
        }
        return toExternal(*result);
    }

    bool create(PhantomHologram hologram) override {
        return hologram::HologramService::getInstance().create(toInternal(hologram));
    }

    bool remove(std::string const& name) override { return hologram::HologramService::getInstance().remove(name); }

    bool rename(std::string const& oldName, std::string const& newName) override {
        return hologram::HologramService::getInstance().rename(oldName, newName);
    }

    bool setEnabled(std::string const& name, bool enabled) override {
        return hologram::HologramService::getInstance().setEnabled(name, enabled);
    }

    bool setPosition(std::string const& name, Vec3 const& position, int dimension) override {
        return hologram::HologramService::getInstance().setPosition(name, position, dimension);
    }

    bool setOptions(std::string const& name, bool enabled, double viewDistance, double lineSpacing) override {
        return hologram::HologramService::getInstance().setOptions(name, enabled, viewDistance, lineSpacing);
    }

    bool setLines(std::string const& name, std::vector<std::string> lines) override {
        return hologram::HologramService::getInstance().setLines(name, std::move(lines));
    }

    bool setLineDynamic(
        std::string const&       name,
        std::size_t              index,
        std::vector<std::string> content,
        std::uint64_t            updateIntervalMs,
        bool                     parseVariables
    ) override {
        return hologram::HologramService::getInstance()
            .setLineDynamic(name, index, std::move(content), updateIntervalMs, parseVariables);
    }

    bool setLineCallback(
        std::string const&              name,
        std::size_t                     index,
        PhantomHologramLineTextCallback callback,
        std::uint64_t                   updateIntervalMs
    ) override {
        return hologram::HologramService::getInstance()
            .setLineCallback(name, index, std::move(callback), updateIntervalMs);
    }

    bool appendLine(std::string const& name, std::string line) override {
        return hologram::HologramService::getInstance().appendLine(name, std::move(line));
    }

    bool insertLine(std::string const& name, std::size_t index, std::string line) override {
        return hologram::HologramService::getInstance().insertLine(name, index, std::move(line));
    }

    bool setLine(std::string const& name, std::size_t index, std::string line) override {
        return hologram::HologramService::getInstance().setLine(name, index, std::move(line));
    }

    bool removeLine(std::string const& name, std::size_t index) override {
        return hologram::HologramService::getInstance().removeLine(name, index);
    }

    bool reload() override { return hologram::HologramService::getInstance().reload(); }

    bool save() override { return hologram::HologramService::getInstance().save(); }

    void invalidate() override {}
};

std::shared_ptr<PhantomHologramService> gPhantomHologramService;

} // namespace

bool registerServices() {
    if (gPhantomHologramService != nullptr) {
        return true;
    }

    auto service = std::make_shared<PhantomHologramServiceImpl>();
    if (!ll::service::ServiceManager::getInstance().registerService(service)) {
        return false;
    }

    gPhantomHologramService = std::move(service);
    return true;
}

void unregisterServices() {
    if (gPhantomHologramService == nullptr) {
        return;
    }

    ll::service::ServiceManager::getInstance().unregisterService(PhantomHologramService::ServiceId);
    gPhantomHologramService.reset();
}

} // namespace phantom::service
