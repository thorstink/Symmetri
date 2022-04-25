#pragma once
#include <iostream>
#include <sstream>
#include <string>

#include "types.h"

namespace symmetri {

std::string activeClass(float alpha) {
  return "classDef transition" + std::to_string((int(10 * alpha))) +
         " fill-opacity:" + std::to_string(1.0 - alpha) + ";\n";
}

const std::string active_transition_class = "classDef active fill:#ffc0cb;\n";
const std::string active_transition_tag = ":::active";
const std::string place_class = "classDef place fill:#f3fff7;\n";
const std::string place_tag = ":::place";

std::string opacity(float alpha) {
  return ":::transition" + std::to_string((int(10 * alpha)));
}
auto footer = []() {
  std::string s;
  for (float alpha = 0.1; alpha <= 1.001;) {
    s += activeClass(alpha);
    alpha += 0.1;
  }
  return s;
}();

std::string multi(uint16_t m) { return "|" + std::to_string(m) + "|"; }

std::string placeFormatter(const std::string &id, int marking = 0) {
  return id + "((" + id + " : " + std::to_string(marking) + "))";
}

float getRatio(
    const clock_t::time_point &now, const Transition &t,
    const std::map<Transition, clock_t::time_point> &transition_end_times) {
  float window = 2.5f;

  return std::min<float>(
             std::chrono::duration<float>(now - transition_end_times.at(t))
                 .count(),
             window) /
         window;
}
const std::string conn = "---";
const std::string header = "graph RL\n";

auto genNet(const clock_t::time_point &now, const StateNet &net,
            const NetMarking &M, std::set<Transition> pending_transitions,
            std::map<symmetri::Transition, symmetri::clock_t::time_point>
                transition_end_times) {
  std::stringstream mermaid;
  mermaid << header;

  for (const auto &[t, mut] : net) {
    const auto &[pre, post] = mut;
    for (auto p = pre.begin(); p != pre.end(); p = pre.upper_bound(*p)) {
      uint16_t marking = M.at(*p);
      size_t count = pre.count(*p);

      float ratio = getRatio(now, t, transition_end_times);

      mermaid << t
              << (pending_transitions.contains(t) ? active_transition_tag
                                                  : opacity(ratio))
              << conn << (count > 1 ? multi(count) : "")
              << placeFormatter(*p, marking) << place_tag << "\n";
    }

    for (auto p = post.begin(); p != post.end(); p = post.upper_bound(*p)) {
      uint16_t marking = M.at(*p);
      size_t count = post.count(*p);
      float ratio = getRatio(now, t, transition_end_times);

      mermaid << placeFormatter(*p, marking) << place_tag << conn
              << (count > 1 ? multi(count) : "") << t
              << (pending_transitions.contains(t) ? active_transition_tag
                                                  : opacity(ratio))
              << "\n";
    }
  }
  mermaid << place_class << active_transition_class << footer;
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
