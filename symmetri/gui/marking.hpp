#pragma once
#include <symmetri/types.h>

#include <iostream>
#include <sstream>

#include "imgui.h"
#include "redux.hpp"

template <>
void draw(symmetri::Marking& marking) {
  if (ImGui::BeginListBox("##marking")) {
    for (const auto& [place, color] : marking) {
      std::stringstream s;
      s << "(" << place << "," << symmetri::Color::toString(color) << ")";
      if (ImGui::Selectable(s.str().c_str(), false)) {
      }
    }
    ImGui::EndListBox();
  }
}
