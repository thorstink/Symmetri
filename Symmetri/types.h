#pragma once

#include <Eigen/SparseCore>
#include <optional>
#include <string>
#include <unordered_map>

namespace symmetri {
using ArcList = std::vector<std::tuple<bool, std::string, std::string>>;
using Transition = std::string;
using Marking = Eigen::SparseVector<int16_t>;
using MarkingMutation = std::unordered_map<std::string, int16_t>;
using TransitionMutation = std::unordered_map<Transition, Marking>;
using Transitions = std::vector<Transition>;

struct Conversions {
 public:
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
  std::string getLabel(Eigen::Index id) const { return id_to_label_.at(id); }
  Eigen::Index getIndex(const std::string &label) const {
    return label_to_id_.at(label);
  }
};

}  // namespace symmetri
