#pragma once

#include "imgui.h"
#include "symmetri/colors.hpp"

void moveView(const ImVec2& d);

void moveNode(bool is_place, size_t idx, const ImVec2& d);

void addNode(bool is_place, ImVec2 pos);

void removeArc(bool is_input, size_t transition_idx, size_t sub_idx);

void addArc(bool is_place, size_t source, size_t target, symmetri::Token color);

void removePlace(size_t idx);

void removeTransition(size_t idx);

void updateTransitionName(const size_t idx, const std::string& d);

void updateTransitionPriority(const size_t idx, const int8_t priority);

void updateArcColor(bool is_input, size_t idx, size_t sub_idx,
                    const symmetri::Token color);

void updatePlaceName(const size_t idx, const std::string& d);

void setContextMenuActive();

void setContextMenuInactive();

void setSelectedNode(bool is_place, size_t idx);

void resetSelection();

void setSelectedArc(bool is_input, size_t idx, size_t sub_idx);

void renderNodeEntry(bool is_place, const std::string& name, size_t idx,
                     bool selected);

int updateActiveFile(ImGuiInputTextCallbackData* data);
