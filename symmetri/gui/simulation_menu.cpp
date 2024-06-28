#include "simulation_menu.h"

#include <ranges>

#include "petri.h"
#include "reducers.h"
void draw_simulation_menu(const model::ViewModel& vm) {
  ImGui::Text("Simulation");
  ImGui::Separator();

  for (auto transition_idx : vm.t_fireable) {
    if (ImGui::Selectable(vm.net.transition[transition_idx].c_str())) {
      tryFire(transition_idx);
    } else if (ImGui::IsItemHovered() &&
               (not vm.selected_target_node_idx.has_value() ||
                vm.selected_target_node_idx.value() !=
                    std::tuple<bool, size_t>{false, transition_idx})) {
      setSelectedTargetNode(false, transition_idx);
    }
  }
  ImGui::Dummy(ImVec2(0.0f, 20.0f));
}
