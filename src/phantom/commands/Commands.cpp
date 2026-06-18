#include "phantom/commands/Commands.h"

#include "mod/Phantom.h"
#include "phantom/hologram/HologramService.h"
#include "phantom/i18n/I18n.h"
#include "phantom/ui/HologramForms.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOriginType.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace phantom::commands {
namespace detail {

enum class PhantomAction : unsigned char {
    gui,
    list,
    reload,
    create,
    remove,
    movehere,
    enable,
    disable,
    append,
    setline,
    dynamicline,
    removeline,
};

struct EmptyParam {};

struct ActionParam {
    PhantomAction action;
};

struct NamedActionParam {
    PhantomAction action{};
    std::string   name;
};

struct TextActionParam {
    PhantomAction action{};
    std::string   name;
    std::string   text;
};

struct LineActionParam {
    PhantomAction action{};
    std::string   name;
    int           index{};
};

struct SetLineActionParam {
    PhantomAction action{};
    std::string   name;
    int           index{};
    std::string   text;
};

struct DynamicLineActionParam {
    PhantomAction action{};
    std::string   name;
    int           index{};
    int           intervalMs{};
    int           parseVariables{};
    std::string   text;
};

[[nodiscard]] Player* getPlayer(CommandOrigin const& origin) {
    if (origin.getOriginType() != CommandOriginType::Player) {
        return nullptr;
    }
    auto* actor = origin.getEntity();
    if (actor == nullptr || !actor->isPlayer()) {
        return nullptr;
    }
    return static_cast<Player*>(actor);
}

[[nodiscard]] std::string tr(CommandOrigin const& origin, std::string_view key) {
    auto locale = origin.getLocaleCode();
    if (locale.empty()) {
        locale = Phantom::getInstance().getLanguage();
    }
    return i18n::tr(key, locale);
}

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

[[nodiscard]] std::string
trf(CommandOrigin const&                                            origin,
    std::string_view                                                key,
    std::initializer_list<std::pair<std::string_view, std::string>> args) {
    auto text = tr(origin, key);
    for (auto const& [name, value] : args) {
        std::string placeholder  = "{";
        placeholder             += name;
        placeholder             += "}";
        replaceAll(text, placeholder, value);
    }
    return text;
}

void listHolograms(CommandOrigin const& origin, CommandOutput& output) {
    auto holograms = hologram::HologramService::getInstance().list();
    if (holograms.empty()) {
        output.success(tr(origin, "phantom.command.empty"));
        return;
    }
    std::stringstream stream;
    stream << trf(
        origin,
        "phantom.command.list_header",
        {
            {"count", std::to_string(holograms.size())}
    }
    );
    for (auto const& hologram : holograms) {
        stream << '\n'
               << trf(origin,
                      "phantom.command.list_item",
                      {
                          {"name", hologram.name},
                          {"state",
                           tr(origin, hologram.enabled ? "phantom.command.state_on" : "phantom.command.state_off")},
                          {"lines", std::to_string(hologram.lines.size())},
                          {"dimension", std::to_string(hologram.dimension)},
                          {"x", std::to_string(hologram.position.x)},
                          {"y", std::to_string(hologram.position.y)},
                          {"z", std::to_string(hologram.position.z)},
        });
    }
    output.success(stream.str());
}

void openGui(CommandOrigin const& origin, CommandOutput& output) {
    if (!Phantom::getInstance().getConfig().forms) {
        listHolograms(origin, output);
        return;
    }
    auto* player = getPlayer(origin);
    if (player == nullptr) {
        output.error(tr(origin, "phantom.command.player_only"));
        return;
    }
    ui::openMain(*player);
}

void handleAction(CommandOrigin const& origin, CommandOutput& output, PhantomAction action) {
    switch (action) {
    case PhantomAction::gui:
        openGui(origin, output);
        break;
    case PhantomAction::list:
        listHolograms(origin, output);
        break;
    case PhantomAction::reload:
        hologram::HologramService::getInstance().reload();
        output.success(tr(origin, "phantom.command.reloaded"));
        break;
    default:
        output.error(tr(origin, "phantom.command.requires_more_args"));
        break;
    }
}

void handleNamed(CommandOrigin const& origin, CommandOutput& output, PhantomAction action, std::string const& name) {
    auto& service = hologram::HologramService::getInstance();
    bool  ok      = false;
    switch (action) {
    case PhantomAction::remove:
        ok = service.remove(name);
        output.success(ok ? tr(origin, "phantom.command.deleted") : tr(origin, "phantom.command.not_found"));
        break;
    case PhantomAction::movehere:
        if (auto* player = getPlayer(origin)) {
            ok = service.moveNearPlayer(name, *player);
            output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
        } else {
            output.error(tr(origin, "phantom.command.player_only"));
        }
        break;
    case PhantomAction::enable:
        ok = service.setEnabled(name, true);
        output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
        break;
    case PhantomAction::disable:
        ok = service.setEnabled(name, false);
        output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
        break;
    default:
        output.error(tr(origin, "phantom.command.unsupported_named_action"));
        break;
    }
}

void handleText(
    CommandOrigin const& origin,
    CommandOutput&       output,
    PhantomAction        action,
    std::string const&   name,
    std::string const&   text
) {
    auto& service = hologram::HologramService::getInstance();
    bool  ok      = false;
    switch (action) {
    case PhantomAction::create: {
        auto* player = getPlayer(origin);
        if (player == nullptr) {
            output.error(tr(origin, "phantom.command.player_only"));
            return;
        }
        auto pos  = player->getPosition();
        pos.y    += 2.2f;
        hologram::Hologram hologram;
        hologram.name      = name;
        hologram.position  = pos;
        hologram.dimension = static_cast<int>(player->getDimensionId());
        hologram.lines.push_back({text});
        ok = service.create(std::move(hologram));
        output.success(ok ? tr(origin, "phantom.command.created") : tr(origin, "phantom.command.exists"));
        break;
    }
    case PhantomAction::append:
        ok = service.appendLine(name, text);
        output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
        break;
    default:
        output.error(tr(origin, "phantom.command.unsupported_text_action"));
        break;
    }
}

void handleLine(
    CommandOrigin const& origin,
    CommandOutput&       output,
    PhantomAction        action,
    std::string const&   name,
    int                  index
) {
    if (index < 1) {
        output.error(tr(origin, "phantom.command.line_index_starts_at_one"));
        return;
    }
    if (action != PhantomAction::removeline) {
        output.error(tr(origin, "phantom.command.unsupported_line_action"));
        return;
    }
    auto ok = hologram::HologramService::getInstance().removeLine(name, static_cast<std::size_t>(index - 1));
    output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
}

void handleSetLine(
    CommandOrigin const& origin,
    CommandOutput&       output,
    std::string const&   name,
    int                  index,
    std::string const&   text
) {
    if (index < 1) {
        output.error(tr(origin, "phantom.command.line_index_starts_at_one"));
        return;
    }
    auto ok = hologram::HologramService::getInstance().setLine(name, static_cast<std::size_t>(index - 1), text);
    output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
}

std::vector<std::string> splitVariants(std::string const& text) {
    std::vector<std::string> result;
    std::string              current;
    for (auto ch : text) {
        if (ch == '|') {
            result.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    result.push_back(std::move(current));
    return result;
}

void handleDynamicLine(
    CommandOrigin const& origin,
    CommandOutput&       output,
    std::string const&   name,
    int                  index,
    int                  intervalMs,
    int                  parseVariables,
    std::string const&   text
) {
    if (index < 1) {
        output.error(tr(origin, "phantom.command.line_index_starts_at_one"));
        return;
    }
    auto ok = hologram::HologramService::getInstance().setLineDynamic(
        name,
        static_cast<std::size_t>(index - 1),
        splitVariants(text),
        static_cast<uint64_t>(std::max(0, intervalMs)),
        parseVariables != 0
    );
    output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
}

} // namespace detail

void registerCommands() {
    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "phantom",
        i18n::tr("phantom.command.description", Phantom::getInstance().getLanguage()),
        CommandPermissionLevel::GameDirectors
    );

    command.alias("hologram");
    command.alias("holo");

    command.overload<detail::EmptyParam>().execute([](CommandOrigin const& origin,
                                                      CommandOutput&       output,
                                                      detail::EmptyParam const&) { detail::openGui(origin, output); });
    command.overload<detail::ActionParam>().required("action").execute(
        [](CommandOrigin const& origin, CommandOutput& output, detail::ActionParam const& param) {
            detail::handleAction(origin, output, param.action);
        }
    );
    command.overload<detail::NamedActionParam>().required("action").required("name").execute(
        [](CommandOrigin const& origin, CommandOutput& output, detail::NamedActionParam const& param) {
            detail::handleNamed(origin, output, param.action, param.name);
        }
    );
    command.overload<detail::TextActionParam>().required("action").required("name").required("text").execute(
        [](CommandOrigin const& origin, CommandOutput& output, detail::TextActionParam const& param) {
            detail::handleText(origin, output, param.action, param.name, param.text);
        }
    );
    command.overload<detail::LineActionParam>().required("action").required("name").required("index").execute(
        [](CommandOrigin const& origin, CommandOutput& output, detail::LineActionParam const& param) {
            detail::handleLine(origin, output, param.action, param.name, param.index);
        }
    );
    command.overload<detail::SetLineActionParam>()
        .required("action")
        .required("name")
        .required("index")
        .required("text")
        .execute([](CommandOrigin const& origin, CommandOutput& output, detail::SetLineActionParam const& param) {
            detail::handleSetLine(origin, output, param.name, param.index, param.text);
        });
    command.overload<detail::DynamicLineActionParam>()
        .required("action")
        .required("name")
        .required("index")
        .required("intervalMs")
        .required("parseVariables")
        .required("text")
        .execute([](CommandOrigin const& origin, CommandOutput& output, detail::DynamicLineActionParam const& param) {
            detail::handleDynamicLine(
                origin,
                output,
                param.name,
                param.index,
                param.intervalMs,
                param.parseVariables,
                param.text
            );
        });
}

} // namespace phantom::commands
