#pragma once

#include <memory>

#include "imgui.h"
#include "model.h"
#include "rxdispatch.h"

inline void drawUi(const std::shared_ptr<model::ViewModel>& vm) {
  // Global undo/redo: Cmd-Z / Cmd-Shift-Z on macOS, Ctrl-Z / Ctrl-Shift-Z
  // elsewhere (ImGuiMod_Shortcut). Skipped while a text field wants the keys so
  // it keeps its own in-field undo.
  if (!ImGui::GetIO().WantTextInput) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Shortcut | ImGuiMod_Shift |
                                 ImGuiKey_Z)) {
      rxdispatch::pushRedo();
    } else if (ImGui::IsKeyChordPressed(ImGuiMod_Shortcut | ImGuiKey_Z)) {
      rxdispatch::pushUndo();
    }
  }

  ImGui::Begin("main", NULL);
  for (auto drawable : vm->drawables) {
    drawable(*vm);
  }
  ImGui::End();
}
