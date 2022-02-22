#pragma once
#include <string>

#include "types.h"

namespace symmetri {

const std::string active = "active";
const std::string active_tag = ":::" + active;
const std::string active_def = "classDef " + active + " fill:#6fd0a7;\n";
const std::string passive = "passive";
const std::string passive_tag = ":::" + passive;
const std::string passive_def = "classDef " + passive + " fill:#bad1ce;\n";
const std::string conn = "---";
const std::string header = "graph RL\n" + active_def + passive_def;

std::string placeFormatter(const std::string &id, int marking = 0) {
  return id + "((" + id + " : " + std::to_string(marking) + "))";
}

auto getPlaceMarking(
    const std::map<Eigen::Index, std::string> &index_marking_map,
    const Marking &marking) {
  std::map<std::string, int> id_marking_map;
  for (const auto &[idx, id] : index_marking_map) {
    id_marking_map.emplace(id, marking.coeff(idx));
  }
  return id_marking_map;
}

auto genNet(const ArcList &arc_list,
            const std::map<std::string, int> &id_marking_map,
            const std::set<std::string> &active_transitions) {
  std::stringstream mermaid;
  mermaid << header;

  for (const auto &[to_place, p, t] : arc_list) {
    int marking = id_marking_map.at(p);
    if (to_place) {
      mermaid << t
              << (active_transitions.contains(t) ? active_tag : passive_tag)
              << conn << placeFormatter(p, marking) << "\n";
    } else {
      mermaid << placeFormatter(p, marking) << conn << t
              << (active_transitions.contains(t) ? active_tag : passive_tag)
              << "\n";
    }
  }
  return mermaid.str();
}

auto logToCsv(
    const std::multimap<symmetri::Transition, symmetri::TaskInstance> &log) {
  std::stringstream log_data;

  for (auto &&[task_id, task_instance] : log) {
    auto &&[start, end, thread_id] = task_instance;
    log_data << thread_id << ','
             << std::chrono::duration_cast<std::chrono::milliseconds>(
                    start.time_since_epoch())
                    .count()
             << ','
             << std::chrono::duration_cast<std::chrono::milliseconds>(
                    end.time_since_epoch())
                    .count()
             << ',' << task_id << '\n';
  }
  return log_data.str();
};

}  // namespace symmetri
