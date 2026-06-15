#include "phantom/ui/HologramForms.h"

#include "phantom/hologram/HologramService.h"
#include "phantom/i18n/I18n.h"

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

[[nodiscard]] std::string tr(Player const& player, std::string_view key) {
    return i18n::tr(key, player.getLocaleCode());
}

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

[[nodiscard]] std::string format(std::string value, std::initializer_list<std::pair<std::string_view, std::string>> args) {
    for (auto const& [key, replacement] : args) {
        std::string placeholder = "{";
        placeholder += key;
        placeholder += "}";
        replaceAll(value, placeholder, replacement);
    }
    return value;
}

[[nodiscard]] std::string trf(
    Player const&                                                   player,
    std::string_view                                                key,
    std::initializer_list<std::pair<std::string_view, std::string>> args
) {
    return format(tr(player, key), args);
}

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

[[nodiscard]] int parseInt(std::string const& value, int fallback) {
    int  parsed = fallback;
    auto first  = value.data();
    auto last   = value.data() + value.size();
    auto result = std::from_chars(first, last, parsed);
    return result.ec == std::errc{} ? parsed : fallback;
}

[[nodiscard]] uint64_t parseUint64(std::string const& value, uint64_t fallback) {
    uint64_t parsed = fallback;
    auto     first  = value.data();
    auto     last   = value.data() + value.size();
    auto     result = std::from_chars(first, last, parsed);
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

[[nodiscard]] std::string joinContent(phantom::hologram::HologramLine const& line) {
    auto const& content = line.content.empty() ? std::vector<std::string>{line.text} : line.content;
    std::string joined;
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (i != 0) {
            joined += '\n';
        }
        joined += content[i];
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
        return dim == 0 ? "Overworld" : "Dimension " + std::to_string(dim);
    }
}

} // namespace

void openMain(Player& player) {
    auto holograms = hologram::HologramService::getInstance().list();
    std::ranges::sort(holograms, {}, &hologram::Hologram::name);

    ll::form::SimpleForm form{tr(player, "phantom.form.main.title"), tr(player, "phantom.form.main.content")};
    form.appendButton(tr(player, "phantom.form.main.create"), [](Player& p) { openCreate(p); });
    form.appendButton(tr(player, "phantom.form.main.reload"), [](Player& p) {
        hologram::HologramService::getInstance().reload();
        openMain(p);
    });
    for (auto const& hologram : holograms) {
        auto label = hologram.enabled ? hologram.name
                                      : hologram.name + " (" + tr(player, "phantom.form.state.disabled") + ")";
        form.appendButton(label, [name = hologram.name](Player& p) { openEditor(p, name); });
    }
    form.sendTo(player);
}

void openCreate(Player& player) {
    auto pos = player.getPosition();
    ll::form::CustomForm form{tr(player, "phantom.form.create.title")};
    form.appendInput("name", tr(player, "phantom.form.field.name"), "spawn", "hologram");
    form.appendInput(
        "text",
        tr(player, "phantom.form.field.lines"),
        tr(player, "phantom.form.field.lines.placeholder"),
        "Welcome"
    );
    form.appendInput("x", "X", "", std::to_string(pos.x));
    form.appendInput("y", "Y", "", std::to_string(pos.y + 2.2f));
    form.appendInput("z", "Z", "", std::to_string(pos.z));
    form.appendInput(
        "dim",
        tr(player, "phantom.form.field.dimension"),
        tr(player, "phantom.form.field.dimension.placeholder"),
        std::to_string(static_cast<int>(player.getDimensionId()))
    );
    form.appendToggle("enabled", tr(player, "phantom.form.field.enabled"), true);
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
        hologram.dimension = parseInt(valueOr<std::string>(result, "dim", "0"), static_cast<int>(p.getDimensionId()));
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
        trf(player, "phantom.form.edit.title", {{"name", hologram->name}}),
        trf(player,
            "phantom.form.edit.content",
            {{"lines", std::to_string(hologram->lines.size())}, {"dimension", dimName(hologram->dimension)}})
    };
    form.appendButton(tr(player, "phantom.form.edit.lines"), [name](Player& p) { openLines(p, name); });
    form.appendButton(tr(player, "phantom.form.edit.dynamic_lines"), [name](Player& p) { openDynamicLines(p, name); });
    form.appendButton(tr(player, "phantom.form.edit.move_to_me"), [name](Player& p) {
        hologram::HologramService::getInstance().moveNearPlayer(name, p);
        openEditor(p, name);
    });
    form.appendButton(
        hologram->enabled ? tr(player, "phantom.form.edit.disable") : tr(player, "phantom.form.edit.enable"),
        [name, enabled = !hologram->enabled](Player& p) {
            hologram::HologramService::getInstance().setEnabled(name, enabled);
            openEditor(p, name);
        }
    );
    form.appendButton(tr(player, "phantom.form.edit.advanced"), [h = *hologram](Player& p) {
        ll::form::CustomForm edit{trf(p, "phantom.form.advanced.title", {{"name", h.name}})};
        edit.appendInput("x", "X", "", std::to_string(h.position.x));
        edit.appendInput("y", "Y", "", std::to_string(h.position.y));
        edit.appendInput("z", "Z", "", std::to_string(h.position.z));
        edit.appendInput(
            "dim",
            tr(p, "phantom.form.field.dimension"),
            tr(p, "phantom.form.field.dimension.custom"),
            std::to_string(h.dimension)
        );
        edit.appendToggle("enabled", tr(p, "phantom.form.field.enabled"), h.enabled);
        edit.appendSlider(
            "view",
            tr(p, "phantom.form.field.view_distance"),
            8.0,
            128.0,
            1.0,
            h.viewDistance > 0.0 ? h.viewDistance : 48.0
        );
        edit.appendSlider(
            "spacing",
            tr(p, "phantom.form.field.line_spacing"),
            0.15,
            0.60,
            0.01,
            h.lineSpacing > 0.0 ? h.lineSpacing : 0.27
        );
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
            auto dim     = parseInt(valueOr<std::string>(result, "dim", "0"), 0);
            auto enabled = valueOr<std::uint64_t>(result, "enabled", 1) != 0;
            auto view    = valueOr<double>(result, "view", 48.0);
            auto spacing = valueOr<double>(result, "spacing", 0.27);
            hologram::HologramService::getInstance().setPosition(name, pos, dim);
            hologram::HologramService::getInstance().setOptions(name, enabled, view, spacing);
            openEditor(p2, name);
        });
    });
    form.appendButton(tr(player, "phantom.form.edit.delete"), [name](Player& p) { openDeleteConfirm(p, name); });
    form.appendButton(tr(player, "phantom.form.back"), [](Player& p) { openMain(p); });
    form.sendTo(player);
}

void openLines(Player& player, std::string name) {
    auto hologram = hologram::HologramService::getInstance().get(name);
    if (!hologram) {
        openMain(player);
        return;
    }
    ll::form::CustomForm form{trf(player, "phantom.form.lines.title", {{"name", name}})};
    form.appendInput(
        "lines",
        tr(player, "phantom.form.field.lines"),
        tr(player, "phantom.form.field.lines.placeholder"),
        joinLines(*hologram)
    );
    form.sendTo(player, [name](Player& p, CustomFormResult const& result, ll::form::FormCancelReason) {
        if (result) {
            hologram::HologramService::getInstance().setLines(name, splitLines(valueOr<std::string>(result, "lines", "")));
        }
        openEditor(p, name);
    });
}

void openDynamicLines(Player& player, std::string name) {
    auto hologram = hologram::HologramService::getInstance().get(name);
    if (!hologram) {
        openMain(player);
        return;
    }
    ll::form::SimpleForm form{
        trf(player, "phantom.form.dynamic.title", {{"name", name}}),
        tr(player, "phantom.form.dynamic.content")
    };
    for (std::size_t i = 0; i < hologram->lines.size(); ++i) {
        auto const& line  = hologram->lines[i];
        auto        label = std::to_string(i + 1) + ". ";
        label += line.updateIntervalMs > 0 || line.parseVariables || line.content.size() > 1
                   ? "[" + tr(player, "phantom.form.state.dynamic") + "] "
                   : "[" + tr(player, "phantom.form.state.static") + "] ";
        label += line.text.empty() ? "(" + tr(player, "phantom.form.empty") + ")" : line.text;
        form.appendButton(label, [name, i](Player& p) { openDynamicLine(p, name, i); });
    }
    form.appendButton(tr(player, "phantom.form.back"), [name](Player& p) { openEditor(p, name); });
    form.sendTo(player);
}

void openDynamicLine(Player& player, std::string name, std::size_t index) {
    auto hologram = hologram::HologramService::getInstance().get(name);
    if (!hologram || index >= hologram->lines.size()) {
        openDynamicLines(player, name);
        return;
    }
    auto const& line = hologram->lines[index];
    ll::form::CustomForm form{
        trf(player, "phantom.form.dynamic.line_title", {{"line", std::to_string(index + 1)}})
    };
    form.appendLabel(tr(player, "phantom.form.dynamic.variables"));
    form.appendInput(
        "content",
        tr(player, "phantom.form.field.content_pool"),
        tr(player, "phantom.form.field.content_pool.placeholder"),
        joinContent(line)
    );
    form.appendInput(
        "interval",
        tr(player, "phantom.form.field.update_interval"),
        tr(player, "phantom.form.field.update_interval.placeholder"),
        std::to_string(line.updateIntervalMs)
    );
    form.appendToggle("parse", tr(player, "phantom.form.field.parse_variables"), line.parseVariables);
    form.sendTo(player, [name, index](Player& p, CustomFormResult const& result, ll::form::FormCancelReason) {
        if (result) {
            auto content  = splitLines(valueOr<std::string>(result, "content", ""));
            auto interval = parseUint64(valueOr<std::string>(result, "interval", "0"), 0);
            auto parse    = valueOr<std::uint64_t>(result, "parse", 0) != 0;
            hologram::HologramService::getInstance().setLineDynamic(name, index, std::move(content), interval, parse);
        }
        openDynamicLines(p, name);
    });
}

void openDeleteConfirm(Player& player, std::string name) {
    ll::form::ModalForm form{
        trf(player, "phantom.form.delete.title", {{"name", name}}),
        tr(player, "phantom.form.delete.content"),
        tr(player, "phantom.form.delete.confirm"),
        tr(player, "phantom.form.delete.cancel")
    };
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
