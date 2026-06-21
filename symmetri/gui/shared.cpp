#include "shared.h"

#include <array>
#include <iterator>
#include <unordered_map>

#include "color.hpp"
#include "imgui.h"

void drawColorDropdownMenu(const std::string& menu_name,
                           const std::vector<std::string_view>& colors,
                           const std::function<void(std::string_view)>& func) {
  if (ImGui::BeginMenu(menu_name.data())) {
    for (const auto& color : colors) {
      ImGui::PushStyleColor(ImGuiCol_Text,
                            getColor(symmetri::Token(color.data())));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                            getColor(symmetri::Token(color.data())));
      if (ImGui::MenuItem(color.data())) {
        func(color);
      }
      ImGui::PopStyleColor();
      ImGui::PopStyleColor();
    }
    ImGui::EndMenu();
  };
}

ImU32 getColor(symmetri::Token token) {
  using namespace symmetri;
  static std::unordered_map<Token, ImU32> color_table;
  const auto ptr = color_table.find(token);
  if (ptr != std::end(color_table)) {
    return ptr->second;
  } else {
    const auto rgb = hsv_to_rgb(ratio(), .3, .99);
    const auto color = IM_COL32(rgb[0], rgb[1], rgb[2], 255);
    color_table.insert({token, color});
    return color;
  }
};

bool IsAnyKeyPressed() {
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
    if (ImGui::IsKeyPressed((ImGuiKey)key)) return true;
  return false;
}
