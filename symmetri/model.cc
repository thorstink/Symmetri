#include "model.h"

#include <iostream>

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

Reducer processTransition(size_t T_i, const std::string &case_id,
                          TransitionState result, size_t thread_id,
                          clock_s::time_point start_time,
                          clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[T_i].begin(),
                            place_list[T_i].end());
    }
    model.event_log.push_back({case_id, model.net.transition[T_i],
                               TransitionState::Started, start_time,
                               thread_id});
    model.event_log.push_back({case_id, model.net.transition[T_i],
                               result == TransitionState::Completed
                                   ? TransitionState::Completed
                                   : TransitionState::Error,
                               end_time, thread_id});
    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), T_i));
    return model;
  };
}

Reducer processTransition(size_t T_i, const Eventlog &new_events,
                          TransitionState result) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[T_i].begin(),
                            place_list[T_i].end());
    }

    for (const auto &e : new_events) {
      model.event_log.push_back(e);
    }

    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), T_i));

    return model;
  };
}

Reducer createReducerForTransition(size_t T_i, const PolyAction &task,
                                   const std::string &case_id) {
  const auto start_time = clock_s::now();
  const auto &[ev, res] = runTransition(task);
  const auto end_time = clock_s::now();
  const auto thread_id = getThreadId();
  return ev.empty() ? processTransition(T_i, case_id, res, thread_id,
                                        start_time, end_time)
                    : processTransition(T_i, ev, res);
}

bool canFire(const SmallVector &pre, const std::vector<size_t> &tokens) {
  return std::all_of(pre.begin(), pre.end(), [&](const auto &m_p) {
    return std::count(tokens.begin(), tokens.end(), m_p) >=
           std::count(pre.begin(), pre.end(), m_p);
  });
};

Model &runAll(Model &model,
              moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
              const StoppablePool &polymorphic_actions,
              const std::string &case_id) {
  model.timestamp = clock_s::now();
  // todo; rangify this.

  // find possible transitions
  llvm::SmallVector<uint8_t, 32> possible_transition_list_n;

  for (const size_t place : model.tokens_n) {
    possible_transition_list_n.insert(possible_transition_list_n.begin(),
                                      model.net.p_to_ts_n[place].begin(),
                                      model.net.p_to_ts_n[place].end());
  }

  // sort transition list according to priority
  std::sort(possible_transition_list_n.begin(),
            possible_transition_list_n.end(), [&](size_t a, size_t b) {
              return model.net.priority[a] > model.net.priority[b];
            });

  for (const size_t T_i : possible_transition_list_n) {
    const auto &pre = model.net.input_n[T_i];
    if (canFire(pre, model.tokens_n)) {
      // deduct the marking
      for (const size_t place : pre) {
        // erase one by one. using std::remove_if would remove all tokens at a
        // particular place.
        model.tokens_n.erase(
            std::find(model.tokens_n.begin(), model.tokens_n.end(), place));
      }

      const auto &task = model.net.store[T_i];

      // if the function is nullopt_t, we short-circuit the
      // marking mutation and do it immediately.
      if constexpr (std::is_same_v<std::nullopt_t, decltype(task)>) {
        model.tokens_n.insert(model.tokens_n.begin(),
                              model.net.output_n[T_i].begin(),
                              model.net.output_n[T_i].end());
      } else {
        polymorphic_actions.enqueue([=, &reducers] {
          reducers.enqueue(createReducerForTransition(T_i, task, case_id));
        });
        model.active_transitions_n.push_back(T_i);
      }
    }
    // exit early if we're out of tokens.
    if (model.tokens_n.size() == 0) {
      break;
    }
  }
  return model;
}

std::vector<Place> Model::getMarking() const {
  std::vector<Place> marking;
  std::transform(tokens_n.cbegin(), tokens_n.cend(),
                 std::back_inserter(marking),
                 [&](auto place_index) { return net.place[place_index]; });
  return marking;
}

std::vector<Transition> Model::getActiveTransitions() const {
  std::vector<Transition> transition_list;
  std::transform(active_transitions_n.cbegin(), active_transitions_n.cend(),
                 std::back_inserter(transition_list),
                 [&](auto place_index) { return net.transition[place_index]; });
  return transition_list;
}

}  // namespace symmetri
