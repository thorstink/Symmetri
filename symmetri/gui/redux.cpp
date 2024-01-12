#include "redux.hpp"

#include "graph.hpp"
void draw(Model& m) {
  for (auto& drawable : m.statics) {
    draw(drawable);
  }
  ImGui::SetNextWindowSize(
      ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
  ImGui::SetNextWindowPos(ImVec2(0, m.menu_height));
  ImGui::Begin("test", NULL, ImGuiWindowFlags_NoTitleBar);
  draw(*m.graph);
  ImGui::End();
}
