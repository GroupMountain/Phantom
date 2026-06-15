#pragma once

#include <string>
#include <string_view>

namespace phantom::i18n {

void setup();
[[nodiscard]] std::string tr(std::string_view key, std::string_view localeCode);

} // namespace phantom::i18n
