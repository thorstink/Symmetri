#pragma once
#include "Symmetri/types.h"
#include "nlohmann/json.hpp"
#include <tuple>

struct Conversions {
  Conversions(const std::map<std::string, Eigen::Index> &label_to_id)
      : label_to_id_(label_to_id) {
    for (auto i = label_to_id_.begin(); i != label_to_id_.end(); ++i)
      id_to_label_[i->second] = i->first;
  }
  Conversions(const std::map<Eigen::Index, std::string> &id_to_label)
      : id_to_label_(id_to_label) {
    for (auto i = id_to_label_.begin(); i != id_to_label_.end(); ++i)
      label_to_id_[i->second] = i->first;
  }
  std::map<std::string, Eigen::Index> label_to_id_;
  std::map<Eigen::Index, std::string> id_to_label_;
  std::string getLabel(Eigen::Index id) { return id_to_label_.at(id); }
  Eigen::Index getLabel(const std::string &label) {
    return label_to_id_.at(label);
  }
};

std::tuple<symmetri::TransitionMutation, symmetri::TransitionMutation,
           symmetri::Marking, nlohmann::json, Conversions, Conversions>
constructTransitionMutationMatrices(std::string file);

nlohmann::json
webMarking(const symmetri::Marking &M,
           const std::map<Eigen::Index, std::string> &index_marking_map);
