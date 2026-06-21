#pragma once

#include "imgui.h"
#include "model.h"

inline void drawUi(const model::ViewModel& vm) {
  ImGui::Begin("main", NULL);
  for (auto drawable : vm.drawables) {
    drawable(vm);
  }
  ImGui::End();
}
