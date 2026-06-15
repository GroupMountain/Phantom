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

#include <sstream>
#include <string>

namespace phantom::commands {
namespace {

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
    removeline,
};

struct EmptyParam {};

struct ActionParam {
    PhantomAction action;
};

struct NamedActionParam {
    PhantomAction action;
    std::string   name;
};

struct TextActionParam {
    PhantomAction action;
    std::string   name;
    std::string   text;
};

struct LineActionParam {
    PhantomAction action;
    std::string   name;
    int           index;
};

struct SetLineActionParam {
    PhantomAction action;
    std::string   name;
    int           index;
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
    return i18n::tr(key, origin.getLocaleCode());
}

void listHolograms(CommandOrigin const& origin, CommandOutput& output) {
    auto holograms = hologram::HologramService::getInstance().list();
    if (holograms.empty()) {
        output.success(tr(origin, "phantom.command.empty"));
        return;
    }
    std::stringstream stream;
    stream << "Phantom holograms (" << holograms.size() << "):";
    for (auto const& hologram : holograms) {
        stream << "\n- " << hologram.name << " [" << (hologram.enabled ? "on" : "off") << "] "
               << "lines=" << hologram.lines.size() << " dim=" << hologram.dimension << " pos=(" << hologram.position.x
               << ", " << hologram.position.y << ", " << hologram.position.z << ")";
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
        output.error("This action requires more arguments.");
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
        output.error("Unsupported named action.");
        break;
    }
}

void handleText(CommandOrigin const& origin, CommandOutput& output, PhantomAction action, std::string const& name, std::string const& text) {
    auto& service = hologram::HologramService::getInstance();
    bool  ok      = false;
    switch (action) {
    case PhantomAction::create: {
        auto* player = getPlayer(origin);
        if (player == nullptr) {
            output.error(tr(origin, "phantom.command.player_only"));
            return;
        }
        auto pos = player->getPosition();
        pos.y += 2.2f;
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
        output.error("Unsupported text action.");
        break;
    }
}

void handleLine(CommandOrigin const& origin, CommandOutput& output, PhantomAction action, std::string const& name, int index) {
    if (index < 1) {
        output.error("Line index starts at 1.");
        return;
    }
    if (action != PhantomAction::removeline) {
        output.error("Unsupported line action.");
        return;
    }
    auto ok = hologram::HologramService::getInstance().removeLine(name, static_cast<std::size_t>(index - 1));
    output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
}

void handleSetLine(CommandOrigin const& origin, CommandOutput& output, std::string const& name, int index, std::string const& text) {
    if (index < 1) {
        output.error("Line index starts at 1.");
        return;
    }
    auto ok = hologram::HologramService::getInstance().setLine(name, static_cast<std::size_t>(index - 1), text);
    output.success(ok ? tr(origin, "phantom.command.updated") : tr(origin, "phantom.command.not_found"));
}

} // namespace

void registerCommands() {
    auto& command = ll::command::CommandRegistrar::getServerInstance()
                        .getOrCreateCommand("phantom", "Manage Phantom holograms", CommandPermissionLevel::GameDirectors);

    command.alias("hologram");
    command.alias("holo");

    command.overload<EmptyParam>().execute([](CommandOrigin const& origin, CommandOutput& output, EmptyParam const&) {
        openGui(origin, output);
    });
    command.overload<ActionParam>().required("action").execute(
        [](CommandOrigin const& origin, CommandOutput& output, ActionParam const& param) {
            handleAction(origin, output, param.action);
        }
    );
    command.overload<NamedActionParam>().required("action").required("name").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NamedActionParam const& param) {
            handleNamed(origin, output, param.action, param.name);
        }
    );
    command.overload<TextActionParam>().required("action").required("name").required("text").execute(
        [](CommandOrigin const& origin, CommandOutput& output, TextActionParam const& param) {
            handleText(origin, output, param.action, param.name, param.text);
        }
    );
    command.overload<LineActionParam>().required("action").required("name").required("index").execute(
        [](CommandOrigin const& origin, CommandOutput& output, LineActionParam const& param) {
            handleLine(origin, output, param.action, param.name, param.index);
        }
    );
    command.overload<SetLineActionParam>().required("action").required("name").required("index").required("text").execute(
        [](CommandOrigin const& origin, CommandOutput& output, SetLineActionParam const& param) {
            handleSetLine(origin, output, param.name, param.index, param.text);
        }
    );
}

} // namespace phantom::commands
