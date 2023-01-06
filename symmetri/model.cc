#include "model.h"

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

PolyAction getTransition(const Store &s, const std::string &t) {
  return std::find_if(s.begin(), s.end(),
                      [&](const auto &t_i) { return t == t_i.first; })
      ->second;
}

Reducer processTransition(const std::string &T_i, const std::string &case_id,
                          TransitionState result, size_t thread_id,
                          clock_s::time_point start_time,
                          clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &post = model.net.at(T_i).second;
      model.tokens.insert(model.tokens.begin(), post.begin(), post.end());
    }
    model.event_log.push_back(
        {case_id, T_i, TransitionState::Started, start_time, thread_id});
    model.event_log.push_back({case_id, T_i,
                               result == TransitionState::Completed
                                   ? TransitionState::Completed
                                   : TransitionState::Error,
                               end_time, thread_id});
    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.pending_transitions.erase(std::find(model.pending_transitions.begin(),
                                              model.pending_transitions.end(),
                                              T_i));
    --model.active_transition_count;
    return model;
  };
}

Reducer processTransition(const std::string &T_i, const Eventlog &new_events,
                          TransitionState result) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &post = model.net.at(T_i).second;
      model.tokens.insert(model.tokens.begin(), post.begin(), post.end());
    }

    for (const auto &e : new_events) {
      model.event_log.push_back(e);
    }

    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.pending_transitions.erase(std::find(model.pending_transitions.begin(),
                                              model.pending_transitions.end(),
                                              T_i));
    --model.active_transition_count;

    return model;
  };
}

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id) {
  const auto start_time = clock_s::now();
  const auto &[ev, res] = runTransition(task);
  const auto end_time = clock_s::now();
  const auto thread_id = getThreadId();
  return ev.empty() ? processTransition(T_i, case_id, res, thread_id,
                                        start_time, end_time)
                    : processTransition(T_i, ev, res);
}

int getPriority(
    const std::vector<std::pair<symmetri::Transition, int8_t>> &priorities,
    const symmetri::Transition &t) {
  auto prio = std::lower_bound(
      priorities.begin(), priorities.end(), t,
      [](const auto &e, const auto &t) { return e.first < t; });
  return prio != priorities.end() && !(t < prio->first) ? prio->second : 0;
};

bool canFire(const std::vector<Transition> &pre,
             const std::vector<Place> &tokens) {
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
  std::vector<Transition> possible_transition_list;
  for (const auto &place : model.tokens) {
    // for all the places that have a token, find the transitions for which it
    // is an input place.
    const auto ptr = std::lower_bound(
        model.reverse_loopup.begin(), model.reverse_loopup.end(), place,
        [](const auto &u, const auto &place) { return u.first < place; });

    if (ptr != model.reverse_loopup.end() && !(place < ptr->first)) {
      possible_transition_list.insert(possible_transition_list.begin(),
                                      ptr->second.begin(), ptr->second.end());
    }
  }

  // sort transition list
  std::sort(possible_transition_list.begin(), possible_transition_list.end(),
            [&](const Transition &a, const Transition &b) {
              return getPriority(model.priority, a) >
                     getPriority(model.priority, b);
            });

  // loop over transitions
  for (const auto &T_i : possible_transition_list) {
    const auto &pre = model.net.at(T_i).first;
    if (canFire(pre, model.tokens)) {
      // deduct the marking
      for (const auto &place : pre) {
        // erase one by one. using std::remove_if would remove all tokens at a
        // particular place.
        model.tokens.erase(
            std::find(model.tokens.begin(), model.tokens.end(), place));
      }

      // if the function is nullopt_t, we short-circuit the
      // marking mutation and do it immediately.
      const auto &task =
          std::find_if(model.store.begin(), model.store.end(),
                       [&](const auto &u) { return u.first == T_i; })
              ->second;

      if constexpr (std::is_same_v<std::nullopt_t, decltype(task)>) {
        const auto &post = model.net.at(T_i).second;
        model.tokens.insert(model.tokens.begin(), post.begin(), post.end());
      } else {
        polymorphic_actions.enqueue([=, T_i = T_i, task = task, &reducers] {
          reducers.enqueue(runTransition(T_i, task, case_id));
        });
        model.pending_transitions.push_back(T_i);
        ++model.active_transition_count;
      }
    }
  }

  return model;
}
}  // namespace symmetri
