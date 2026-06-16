#pragma once

#include "ll/api/service/Service.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace phantom::service {

using PhantomHologramLineTextCallback = std::function<void(Player& player, std::vector<std::string>& currentLines)>;

struct PhantomHologramLine {
    std::string              text;
    std::vector<std::string> content;
    std::uint64_t            updateIntervalMs{0};
    bool                     parseVariables{false};
};

struct PhantomHologram {
    std::string                      name;
    int                              dimension{0};
    Vec3                             position{};
    std::vector<PhantomHologramLine> lines;
    bool                             enabled{true};
    double                           viewDistance{-1.0};
    double                           lineSpacing{-1.0};
    bool                             persistent{true};
};

class PhantomHologramService : public ll::service::ServiceImpl<PhantomHologramService, 1> {
public:
    virtual ~PhantomHologramService() = default;

    virtual std::vector<PhantomHologram>   list() const                                                     = 0;
    virtual std::optional<PhantomHologram> get(std::string const& name) const                               = 0;
    virtual bool                           create(PhantomHologram hologram)                                 = 0;
    virtual bool                           remove(std::string const& name)                                  = 0;
    virtual bool                           rename(std::string const& oldName, std::string const& newName)   = 0;
    virtual bool                           setEnabled(std::string const& name, bool enabled)                = 0;
    virtual bool setPosition(std::string const& name, Vec3 const& position, int dimension)                  = 0;
    virtual bool setOptions(std::string const& name, bool enabled, double viewDistance, double lineSpacing) = 0;
    virtual bool setLines(std::string const& name, std::vector<std::string> lines)                          = 0;
    virtual bool setLineDynamic(
        std::string const&       name,
        std::size_t              index,
        std::vector<std::string> content,
        std::uint64_t            updateIntervalMs,
        bool                     parseVariables
    ) = 0;
    virtual bool setLineCallback(
        std::string const&              name,
        std::size_t                     index,
        PhantomHologramLineTextCallback callback,
        std::uint64_t                   updateIntervalMs
    )                                                                                     = 0;
    virtual bool appendLine(std::string const& name, std::string line)                    = 0;
    virtual bool insertLine(std::string const& name, std::size_t index, std::string line) = 0;
    virtual bool setLine(std::string const& name, std::size_t index, std::string line)    = 0;
    virtual bool removeLine(std::string const& name, std::size_t index)                   = 0;
    virtual bool reload()                                                                 = 0;
    virtual bool save()                                                                   = 0;
};

} // namespace phantom::service
