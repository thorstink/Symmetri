#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"
#include "symmetri/colors.hpp"

ImU32 getColor(symmetri::Token token);

void drawColorDropdownMenu(const std::string& menu_name,
                           const std::vector<std::string_view>& colors,
                           const std::function<void(std::string_view)>& func);

bool IsAnyKeyPressed();

const auto no_move_draw_resize = ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoMove;
