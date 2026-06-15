#pragma once

#include "mc/deps/core/math/Vec3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace phantom::hologram {

struct HologramLine {
    std::string text;
};

struct Hologram {
    std::string               name;
    int                       dimension{0};
    Vec3                      position{};
    std::vector<HologramLine> lines;
    bool                      enabled{true};
    double                    viewDistance{-1.0};
    double                    lineSpacing{-1.0};
    bool                      persistent{true};
};

struct HologramStore {
    int                   version{1};
    std::vector<Hologram> holograms;
};

[[nodiscard]] std::uint64_t runtimeIdFor(std::string const& hologramName, std::size_t lineIndex);
[[nodiscard]] std::int64_t  uniqueIdFor(std::string const& hologramName, std::size_t lineIndex);
[[nodiscard]] std::string   normalizeName(std::string value);

} // namespace phantom::hologram
