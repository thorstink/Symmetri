#include "model.h"

namespace symmetri {

size_t toIndex(const std::vector<std::string> &m, const std::string &s) {
  auto ptr = std::find(m.begin(), m.end(), s);
  return std::distance(m.begin(), ptr);
}

bool canFire(const SmallVector &pre, const std::vector<size_t> &tokens) {
  return std::all_of(pre.begin(), pre.end(), [&](const auto &m_p) {
    return std::count(tokens.begin(), tokens.end(), m_p) >=
           std::count(pre.begin(), pre.end(), m_p);
  });
};

gch::small_vector<uint8_t, 32> possibleTransitions(
    const std::vector<size_t> &tokens,
    const std::vector<SmallVector> &p_to_ts_n,
    const std::vector<int8_t> &priorities) {
  gch::small_vector<uint8_t, 32> possible_transition_list_n;
  for (const size_t place : tokens) {
    for (size_t t : p_to_ts_n[place]) {
      if (std::find(possible_transition_list_n.begin(),
                    possible_transition_list_n.end(),
                    t) == possible_transition_list_n.end()) {
        possible_transition_list_n.push_back(t);
      }
    }
  }

  // sort transition list according to priority
  std::sort(possible_transition_list_n.begin(),
            possible_transition_list_n.end(),
            [&](size_t a, size_t b) { return priorities[a] > priorities[b]; });

  return possible_transition_list_n;
}

Reducer processTransition(size_t t_i, const std::string &case_id, State result,
                          Clock::time_point end_time) {
  return [=](Model &&model) {
    if (result == State::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[t_i].begin(),
                            place_list[t_i].end());
    }

    model.event_log.push_back(
        {case_id, model.net.transition[t_i],
         result == State::Completed ? State::Completed : State::Error,
         end_time});

    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), t_i));
    return std::ref(model);
  };
}

Reducer processTransition(size_t t_i, const Eventlog &new_events,
                          State result) {
  return [=, ev = std::move(new_events)](Model &&model) mutable {
    if (result == State::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[t_i].begin(),
                            place_list[t_i].end());
    }
    model.event_log.insert(model.event_log.end(), ev.begin(), ev.end());
    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), t_i));

    return std::ref(model);
  };
}

Reducer createReducerForTransition(
    size_t t_i, const PolyAction &task, const std::string &case_id,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers) {
  const auto start_time = Clock::now();
  reducers->enqueue([=](Model &&model) {
    model.event_log.push_back(
        {case_id, model.net.transition[t_i], State::Started, start_time});
    return std::ref(model);
  });

  const auto [ev, res] = fireTransition(task);
  const auto end_time = Clock::now();

  return ev.empty() ? processTransition(t_i, case_id, res, end_time)
                    : processTransition(t_i, ev, res);
}

}  // namespace symmetri
