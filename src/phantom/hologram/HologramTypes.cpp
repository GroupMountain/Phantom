#include "phantom/hologram/HologramTypes.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace phantom::hologram {

std::string normalizeName(std::string value) {
    value.erase(
        std::remove_if(
            value.begin(),
            value.end(),
            [](unsigned char ch) { return !std::isalnum(ch) && ch != '_' && ch != '-'; }
        ),
        value.end()
    );
    if (value.empty()) {
        value = "hologram";
    }
    return value;
}

std::uint64_t runtimeIdFor(std::string const& hologramName, std::size_t lineIndex) {
    auto const hash = std::hash<std::string>{}(hologramName + "#" + std::to_string(lineIndex));
    return 0x7F00000000000000ULL | (static_cast<std::uint64_t>(hash) & 0x00FFFFFFFFFFFFFFULL);
}

std::int64_t uniqueIdFor(std::string const& hologramName, std::size_t lineIndex) {
    return -static_cast<std::int64_t>(runtimeIdFor(hologramName, lineIndex) & 0x3FFFFFFFFFFFFFFFULL);
}

} // namespace phantom::hologram
