#include "shared.h"

#include "imgui.h"
#include "symmetri/types.h"

void drawColorDropdownMenu(
    const std::string& menu_name, const std::vector<std::string>& colors,
    const std::function<void(const std::string&)>& func) {
  if (ImGui::BeginMenu(menu_name.c_str())) {
    for (const auto& color : colors) {
      if (ImGui::MenuItem(color.c_str())) {
        func(color);
      }
    }
    ImGui::EndMenu();
  };
}
