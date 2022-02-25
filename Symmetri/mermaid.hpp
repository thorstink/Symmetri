#pragma once
#include <iostream>
#include <sstream>
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

auto genNet(const StateNet &net, const NetMarking &id_marking_map,
            const std::set<std::string> &active_transitions) {
  std::stringstream mermaid;
  mermaid << header;

  for (const auto &[t, mut] : net) {
    const auto &[pre, post] = mut;
    for (const auto &p : pre) {
      int marking = id_marking_map.at(p);
      mermaid << t
              << (active_transitions.contains(t) ? active_tag : passive_tag)
              << conn << placeFormatter(p, marking) << "\n";
    }

    for (const auto &p : post) {
      int marking = id_marking_map.at(p);
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
