#pragma once

#include <memory>

#include "imgui.h"
#include "model.h"
#include "reducers.h"
#include "rxdispatch.h"

inline void drawUi(const std::shared_ptr<model::ViewModel>& vm) {
  // Global shortcuts — skipped while a text field owns the keyboard.
  if (!ImGui::GetIO().WantTextInput) {
    // Undo/redo: Cmd-Z / Cmd-Shift-Z (macOS) or Ctrl-Z / Ctrl-Shift-Z.
    if (ImGui::IsKeyChordPressed(ImGuiMod_Shortcut | ImGuiMod_Shift |
                                 ImGuiKey_Z)) {
      rxdispatch::pushRedo();
    } else if (ImGui::IsKeyChordPressed(ImGuiMod_Shortcut | ImGuiKey_Z)) {
      rxdispatch::pushUndo();
    }

    // Backspace: delete all highlighted nodes.
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
      deleteSelected(vm->t_highlight, vm->p_highlight);
    }
  }

  ImGui::Begin("main", NULL);
  for (auto drawable : vm->drawables) {
    drawable(*vm);
  }
  ImGui::End();

  // Active popups are first-class view state (ViewState::popups). Each renders
  // as a centered, auto-resizing window framed here; its `draw` thunk fills the
  // body (and may dispatch). Closing via the window 'x' removes it by id.
  for (const auto& popup : vm->popups) {
    bool open = true;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin(popup.id.c_str(), &open,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
      popup.draw(*vm);
    }
    ImGui::End();
    if (!open) {
      rxdispatch::pushView(
          [id = popup.id](model::ViewState& v, const model::EditState&) {
            std::erase_if(v.popups,
                          [&](const model::Popup& q) { return q.id == id; });
          });
    }
  }
}
