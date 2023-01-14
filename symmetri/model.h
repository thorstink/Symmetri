#pragma once
#define LLVM_SMALL_VECTOR_IMPLEMENTATION
#include <blockingconcurrentqueue.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "small_vector.hpp"
#include "symmetri/symmetri.h"
namespace symmetri {

using SmallVector = llvm::SmallVector<size_t, 4>;

inline size_t toIndex(const std::vector<std::string> &m, const std::string &s) {
  auto ptr = std::find(m.begin(), m.end(), s);
  return std::distance(m.begin(), ptr);
}

struct Model;
using Reducer = std::function<Model &(Model &&)>;

Reducer createReducerForTransition(size_t T_i, const PolyAction &task,
                                   const std::string &case_id);

struct Model {
  Model(const StateNet &_net, const Store &store,
        const std::vector<std::pair<symmetri::Transition, int8_t>> &_priority,
        const NetMarking &M0)
      : timestamp(clock_s::now()) {
    // populate net:
    const auto transition_count = _net.size();
    net.transition.reserve(transition_count);
    net.store.reserve(transition_count);
    for (const auto &[t, io] : _net) {
      net.transition.push_back(t);
      net.store.push_back(store.at(t));
      for (const auto &p : io.first) {
        net.place.push_back(p);
      }
      for (const auto &p : io.second) {
        net.place.push_back(p);
      }
    }
    {
      // sort and remove duplicates.
      std::sort(net.place.begin(), net.place.end());
      auto last = std::unique(net.place.begin(), net.place.end());
      net.place.erase(last, net.place.end());
    }
    {
      for (const auto &[t, io] : _net) {
        SmallVector q;
        for (const auto &p : io.first) {
          q.push_back(toIndex(net.place, p));
        }
        net.input_n.push_back(q);
        q.clear();
        for (const auto &p : io.second) {
          q.push_back(toIndex(net.place, p));
        }
        net.output_n.push_back(q);
      }
    }
    {
      for (size_t p = 0; p < net.place.size(); p++) {
        SmallVector p_to_ts_n;
        for (size_t c = 0; c < transition_count; c++) {
          for (const auto &input_place : net.input_n[c]) {
            if (p == input_place &&
                std::find(p_to_ts_n.begin(), p_to_ts_n.end(), c) ==
                    p_to_ts_n.end()) {
              p_to_ts_n.push_back(c);
            }
          }
        }
        net.p_to_ts_n.push_back(p_to_ts_n);
      }
    }

    for (const auto &t : net.transition) {
      auto ptr = std::find_if(_priority.begin(), _priority.end(),
                              [t](const auto &a) { return a.first == t; });
      net.priority.push_back(ptr != _priority.end() ? ptr->second : 0);
    }

    // populate initial marking
    for (auto [place, c] : M0) {
      for (int i = 0; i < c; i++) {
        initial_tokens.push_back(toIndex(net.place, place));
      }
    }
    tokens_n = initial_tokens;
  }
  std::vector<Place> getMarking() const;
  std::vector<Transition> getActiveTransitions() const;

  Model &operator=(const Model &) { return *this; }
  Model(const Model &) = delete;

  struct {
    std::vector<Transition> transition;
    std::vector<Place> place;
    std::vector<SmallVector> input_n, output_n, p_to_ts_n;
    std::vector<int8_t> priority;
    std::vector<PolyAction> store;
  } net;

  std::vector<size_t> tokens_n, initial_tokens;
  std::vector<size_t> active_transitions_n;
  clock_s::time_point timestamp;
  Eventlog event_log;
};

Model &runTransitions(Model &model,
                      moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
                      const StoppablePool &polymorphic_actions, bool run_all,
                      const std::string &case_id = "undefined_case_id");

Model &runOnce(Model &model,
               moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
               const StoppablePool &polymorphic_actions,
               const std::string &case_id = "undefined_case_id");

}  // namespace symmetri
