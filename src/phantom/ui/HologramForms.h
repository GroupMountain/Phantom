#pragma once

#include "mc/world/actor/player/Player.h"

#include <string>

namespace phantom::ui {

void openMain(Player& player);
void openCreate(Player& player);
void openEditor(Player& player, std::string name);
void openLines(Player& player, std::string name);
void openDynamicLines(Player& player, std::string name);
void openDynamicLine(Player& player, std::string name, std::size_t index);
void openDeleteConfirm(Player& player, std::string name);

} // namespace phantom::ui
