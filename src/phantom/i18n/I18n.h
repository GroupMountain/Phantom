#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace phantom::i18n {

bool setup(std::filesystem::path const& langDir);
[[nodiscard]] std::string normalizeLocale(std::string_view localeCode);
[[nodiscard]] std::string detectLeviLaminaLocale();
[[nodiscard]] std::string tr(std::string_view key, std::string_view localeCode);

} // namespace phantom::i18n
