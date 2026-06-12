#pragma once

#include <memory>

#include "imgui.h"
#include "model.h"

inline void drawUi(const std::shared_ptr<model::ViewModel>& vm) {
  ImGui::Begin("main", NULL);
  for (auto drawable : vm->drawables) {
    drawable(*vm);
  }
  ImGui::End();
}
