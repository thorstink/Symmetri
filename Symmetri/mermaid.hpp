#pragma once
#include <iostream>
#include <sstream>
#include <string>

#include "Symmetri/types.h"

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

const std::string conn = "-->";
const std::string header = "graph LR\n";

auto genNet(const clock_s::time_point &now, const StateNet &net,
            const NetMarking &M, std::set<Transition> pending_transitions) {
  std::stringstream mermaid;
  mermaid << header;

  for (const auto &[t, mut] : net) {
    auto [pre, post] = mut;
    auto last_pre = std::unique(pre.begin(), pre.end());
    auto last_post = std::unique(post.begin(), post.end());
    pre.erase(last_pre, pre.end());
    post.erase(last_post, post.end());
    for (const auto &p : pre) {
      uint16_t marking = M.at(p);
      size_t count = std::count(std::begin(mut.first), std::end(mut.first), p);

      float ratio = 1.0;

      mermaid << placeFormatter(p, marking) << place_tag << conn
              << (count > 1 ? multi(count) : "") << t
              << (pending_transitions.contains(t) ? active_transition_tag
                                                  : opacity(ratio))
              << "\n";
    }

    for (const auto &p : post) {
      uint16_t marking = M.at(p);
      size_t count =
          std::count(std::begin(mut.second), std::end(mut.second), p);

      float ratio = 1.0;
      mermaid << t
              << (pending_transitions.contains(t) ? active_transition_tag
                                                  : opacity(ratio))
              << conn << (count > 1 ? multi(count) : "")
              << placeFormatter(p, marking) << place_tag << "\n";
    }
  }
  mermaid << place_class << active_transition_class << footer;
  return mermaid.str();
}

std::string stringLogEventlog(const Eventlog &new_events) {
  std::stringstream log_data;
  for (size_t i = 0; i + 1 < new_events.size(); i++) {
    auto start = new_events[i].stamp;
    auto end = new_events[i + 1].stamp;
    if (new_events[i].transition == new_events[i + 1].transition) {
      log_data << new_events[i].thread_id << ','
               << std::chrono::duration_cast<std::chrono::milliseconds>(
                      start.time_since_epoch())
                      .count()
               << ','
               << std::chrono::duration_cast<std::chrono::milliseconds>(
                      end.time_since_epoch())
                      .count()
               << ',' << new_events[i].transition << '\n';
    }
  }
  return log_data.str();
}

}  // namespace symmetri
