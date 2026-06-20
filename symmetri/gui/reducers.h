#pragma once

#include <stddef.h>

#include <string>

#include "gui/model.h"
#include "imgui.h"
#include "petri.h"
#include "symmetri/colors.hpp"

void moveView(const ImVec2& d);

void moveNode(const std::vector<size_t>& t_idxs,
              const std::vector<size_t>& p_idxs, const ImVec2& d);

void addNode(model::Model::NodeType node_type, ImVec2 pos);

void removeArc(model::Model::NodeType source_node_type, size_t transition_idx,
               size_t sub_idx);

void addArc(model::Model::NodeType node_type, size_t source, size_t target,
            symmetri::Token color);

void removePlace(size_t idx);

void removeTransition(size_t idx);

void showGrid(bool show_grid);

void updateArcColor(model::Model::NodeType source_node_type, size_t idx,
                    size_t sub_idx, const symmetri::Token color);

int updatePlaceName(ImGuiInputTextCallbackData* id);

int updateTransitionName(ImGuiInputTextCallbackData* id);

int updatePriority(ImGuiInputTextCallbackData* data);

void setContextMenuActive(ImVec2 world_pos);

void setContextMenuInactive();

void setSelectedNode(model::Model::NodeType node_type, size_t idx);

void addHighlightNode(model::Model::NodeType node_type, size_t idx);

void setSelectedTargetNode(model::Model::NodeType node_type, size_t idx);

void resetSelectedTargetNode();

void resetSelection();

void resetNetView();

void addHighlightArc(model::Model::NodeType source_node_type, size_t t_idx,
                     size_t sub_idx);

void setSelectedArc(model::Model::NodeType source_node_type, size_t source_idx,
                    size_t target_idx);

void renderNodeEntry(model::Model::NodeType node_type, const std::string& name,
                     size_t idx, bool selected);

int updateActiveFile(ImGuiInputTextCallbackData* data);

void updateColorTable();

void setArcHoverState(bool);

void addTokenToPlace(symmetri::AugmentedToken);

void removeTokenFromPlace(symmetri::AugmentedToken);

void tryFire(size_t transition_idx);

void updateTransitionOutputColor(size_t transition_idx, symmetri::Token color);

void deleteSelected(const std::vector<size_t>& t_highlight,
                    const std::vector<size_t>& p_highlight);

void zoomRelative(float);
void zoomAbsolute(float);

// Open a named popup with a render thunk (deduped by id); close it by id. The
// thunk renders the popup body (a window framed by drawUi) and may dispatch.
void addPopup(std::string id,
              std::function<void(const model::ViewModel&)> draw);
void removePopup(std::string id);
