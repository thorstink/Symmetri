#include "shared.h"

#include "color.hpp"
#include "imgui.h"
#include "symmetri/types.h"

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
    const auto rgb = hsv_to_rgb(ratio(), 1., 1.);
    const auto color = IM_COL32(rgb[0], rgb[1], rgb[2], 128);
    color_table.insert({token, color});
    return color;
  }
};
