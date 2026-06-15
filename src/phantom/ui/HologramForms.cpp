#include "phantom/ui/HologramForms.h"

#include "phantom/hologram/HologramService.h"

#include "ll/api/form/CustomForm.h"
#include "ll/api/form/ModalForm.h"
#include "ll/api/form/SimpleForm.h"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <variant>

namespace phantom::ui {
namespace {

using ll::form::CustomFormElementResult;
using ll::form::CustomFormResult;

template <class T>
[[nodiscard]] T valueOr(CustomFormResult const& result, std::string const& key, T fallback) {
    if (!result) {
        return fallback;
    }
    auto iter = result->find(key);
    if (iter == result->end()) {
        return fallback;
    }
    if (auto const* value = std::get_if<T>(&iter->second)) {
        return *value;
    }
    return fallback;
}

[[nodiscard]] std::vector<std::string> splitLines(std::string const& raw) {
    std::vector<std::string> lines;
    std::stringstream        stream{raw};
    std::string              line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

[[nodiscard]] float parseFloat(std::string const& value, float fallback) {
    float parsed = fallback;
    auto  first  = value.data();
    auto  last   = value.data() + value.size();
    auto  result = std::from_chars(first, last, parsed);
    return result.ec == std::errc{} ? parsed : fallback;
}

[[nodiscard]] std::string joinLines(phantom::hologram::Hologram const& hologram) {
    std::string joined;
    for (std::size_t i = 0; i < hologram.lines.size(); ++i) {
        if (i != 0) {
            joined += '\n';
        }
        joined += hologram.lines[i].text;
    }
    return joined;
}

[[nodiscard]] std::string dimName(int dim) {
    switch (dim) {
    case 1:
        return "Nether";
    case 2:
        return "The End";
    default:
        return "Overworld";
    }
}

} // namespace

void openMain(Player& player) {
    auto holograms = hologram::HologramService::getInstance().list();
    std::ranges::sort(holograms, {}, &hologram::Hologram::name);

    ll::form::SimpleForm form{"Phantom", "Hologram manager"};
    form.appendButton("+ Create", [](Player& p) { openCreate(p); });
    form.appendButton("Reload", [](Player& p) {
        hologram::HologramService::getInstance().reload();
        openMain(p);
    });
    for (auto const& hologram : holograms) {
        auto label = hologram.enabled ? hologram.name : hologram.name + " (disabled)";
        form.appendButton(label, [name = hologram.name](Player& p) { openEditor(p, name); });
    }
    form.sendTo(player);
}

void openCreate(Player& player) {
    auto pos = player.getPosition();
    ll::form::CustomForm form{"Create hologram"};
    form.appendInput("name", "Name", "spawn", "hologram");
    form.appendInput("text", "Lines", "One line per row", "Welcome");
    form.appendInput("x", "X", "", std::to_string(pos.x));
    form.appendInput("y", "Y", "", std::to_string(pos.y + 2.2f));
    form.appendInput("z", "Z", "", std::to_string(pos.z));
    form.appendDropdown("dim", "Dimension", {"Overworld", "Nether", "The End"}, static_cast<size_t>(std::clamp(static_cast<int>(player.getDimensionId()), 0, 2)));
    form.appendToggle("enabled", "Enabled", true);
    form.sendTo(player, [](Player& p, CustomFormResult const& result, ll::form::FormCancelReason) {
        if (!result) {
            openMain(p);
            return;
        }
        hologram::Hologram hologram;
        hologram.name      = valueOr<std::string>(result, "name", "hologram");
        hologram.position  = {
            parseFloat(valueOr<std::string>(result, "x", "0"), 0.0f),
            parseFloat(valueOr<std::string>(result, "y", "0"), 0.0f),
            parseFloat(valueOr<std::string>(result, "z", "0"), 0.0f)
        };
        hologram.dimension = static_cast<int>(valueOr<std::uint64_t>(result, "dim", 0));
        hologram.enabled   = valueOr<std::uint64_t>(result, "enabled", 1) != 0;
        for (auto& line : splitLines(valueOr<std::string>(result, "text", ""))) {
            hologram.lines.push_back({std::move(line)});
        }
        hologram::HologramService::getInstance().create(std::move(hologram));
        openMain(p);
    });
}

void openEditor(Player& player, std::string name) {
    auto hologram = hologram::HologramService::getInstance().get(name);
    if (!hologram) {
        openMain(player);
        return;
    }

    ll::form::SimpleForm form{
        "Edit " + hologram->name,
        "Lines: " + std::to_string(hologram->lines.size()) + "\nDimension: " + dimName(hologram->dimension)
    };
    form.appendButton("Edit lines", [name](Player& p) { openLines(p, name); });
    form.appendButton("Move to me", [name](Player& p) {
        hologram::HologramService::getInstance().moveNearPlayer(name, p);
        openEditor(p, name);
    });
    form.appendButton(hologram->enabled ? "Disable" : "Enable", [name, enabled = !hologram->enabled](Player& p) {
        hologram::HologramService::getInstance().setEnabled(name, enabled);
        openEditor(p, name);
    });
    form.appendButton("Advanced", [h = *hologram](Player& p) {
        ll::form::CustomForm edit{"Advanced " + h.name};
        edit.appendInput("x", "X", "", std::to_string(h.position.x));
        edit.appendInput("y", "Y", "", std::to_string(h.position.y));
        edit.appendInput("z", "Z", "", std::to_string(h.position.z));
        edit.appendDropdown("dim", "Dimension", {"Overworld", "Nether", "The End"}, static_cast<size_t>(std::clamp(h.dimension, 0, 2)));
        edit.appendToggle("enabled", "Enabled", h.enabled);
        edit.appendSlider("view", "View distance", 8.0, 128.0, 1.0, h.viewDistance > 0.0 ? h.viewDistance : 48.0);
        edit.appendSlider("spacing", "Line spacing", 0.15, 0.60, 0.01, h.lineSpacing > 0.0 ? h.lineSpacing : 0.27);
        edit.sendTo(p, [name = h.name](Player& p2, CustomFormResult const& result, ll::form::FormCancelReason) {
            if (!result) {
                openEditor(p2, name);
                return;
            }
            auto pos = Vec3{
                parseFloat(valueOr<std::string>(result, "x", "0"), 0.0f),
                parseFloat(valueOr<std::string>(result, "y", "0"), 0.0f),
                parseFloat(valueOr<std::string>(result, "z", "0"), 0.0f)
            };
            auto dim     = static_cast<int>(valueOr<std::uint64_t>(result, "dim", 0));
            auto enabled = valueOr<std::uint64_t>(result, "enabled", 1) != 0;
            auto view    = valueOr<double>(result, "view", 48.0);
            auto spacing = valueOr<double>(result, "spacing", 0.27);
            hologram::HologramService::getInstance().setPosition(name, pos, dim);
            hologram::HologramService::getInstance().setOptions(name, enabled, view, spacing);
            openEditor(p2, name);
        });
    });
    form.appendButton("Delete", [name](Player& p) { openDeleteConfirm(p, name); });
    form.appendButton("< Back", [](Player& p) { openMain(p); });
    form.sendTo(player);
}

void openLines(Player& player, std::string name) {
    auto hologram = hologram::HologramService::getInstance().get(name);
    if (!hologram) {
        openMain(player);
        return;
    }
    ll::form::CustomForm form{"Lines " + name};
    form.appendInput("lines", "Lines", "One line per row", joinLines(*hologram));
    form.sendTo(player, [name](Player& p, CustomFormResult const& result, ll::form::FormCancelReason) {
        if (result) {
            hologram::HologramService::getInstance().setLines(name, splitLines(valueOr<std::string>(result, "lines", "")));
        }
        openEditor(p, name);
    });
}

void openDeleteConfirm(Player& player, std::string name) {
    ll::form::ModalForm form{"Delete " + name, "This removes the hologram from storage.", "Delete", "Cancel"};
    form.sendTo(player, [name](Player& p, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
        if (result && *result == ll::form::ModalFormSelectedButton::Upper) {
            hologram::HologramService::getInstance().remove(name);
            openMain(p);
            return;
        }
        openEditor(p, name);
    });
}

} // namespace phantom::ui
