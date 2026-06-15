#pragma once

#include "mc/world/actor/player/Player.h"

#include <string>

namespace phantom::ui {

void openMain(Player& player);
void openCreate(Player& player);
void openEditor(Player& player, std::string name);
void openLines(Player& player, std::string name);
void openLineMenu(Player& player, std::string name, std::size_t index);
void openLineText(Player& player, std::string name, std::size_t index);
void openAddLine(Player& player, std::string name);
void openRemoveLineConfirm(Player& player, std::string name, std::size_t index);
void openDynamicLines(Player& player, std::string name);
void openDynamicLine(Player& player, std::string name, std::size_t index);
void openDynamicFrames(Player& player, std::string name, std::size_t index);
void openDynamicFrame(Player& player, std::string name, std::size_t index, std::size_t frameIndex);
void openAddDynamicFrame(Player& player, std::string name, std::size_t index);
void openRemoveDynamicFrameConfirm(Player& player, std::string name, std::size_t index, std::size_t frameIndex);
void openDynamicPlayback(Player& player, std::string name, std::size_t index);
void openDeleteConfirm(Player& player, std::string name);

} // namespace phantom::ui
