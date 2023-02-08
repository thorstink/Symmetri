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

using SmallVector = gch::small_vector<size_t, 4>;

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
        const NetMarking &M0);
  std::vector<Place> getMarking() const;
  std::vector<Transition> getActiveTransitions() const;
  std::vector<Transition> getFireableTransitions() const;
  std::pair<std::vector<Transition>, std::vector<Place>> getState() const;

  bool runTransition(
      const Transition &t,
      std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>> reducers,
      const StoppablePool &polymorphic_actions,
      const std::string &case_id = "undefined_case_id");
  void runTransitions(
      std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>> reducers,
      const StoppablePool &polymorphic_actions, bool run_all = true,
      const std::string &case_id = "undefined_case_id");

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

 private:
  bool tryFire(
      const size_t t,
      std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>> reducers,
      const StoppablePool &polymorphic_actions, const std::string &case_id);
};

}  // namespace symmetri
