#pragma once
#include <symmetri/types.h>

#include <iostream>
#include <sstream>

#include "imgui.h"

using Mutation =
    std::pair<std::vector<std::pair<symmetri::Place, symmetri::Token>>,
              std::vector<std::pair<symmetri::Place, symmetri::Token>>>;

static void showMutation(const Mutation& mutation) {
  std::stringstream s;
  for (const auto& input : mutation.first) {
    s << "(" << input.first << "," << symmetri::Color::toString(input.second)
      << ")";
  }
  s << " -> ";
  for (const auto& output : mutation.second) {
    s << "(" << output.first << "," << symmetri::Color::toString(output.second)
      << ")";
  }
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(s.str().c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

template <>
void draw(symmetri::Net& net) {
  if (ImGui::BeginListBox("##bla")) {
    for (const auto& [t, p] : net) {
      if (ImGui::Selectable(t.c_str(), false)) {
      }
      showMutation(p);
    }
    ImGui::EndListBox();
  }
}
