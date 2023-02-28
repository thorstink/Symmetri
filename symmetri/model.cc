#include "model.h"

namespace symmetri {

size_t toIndex(const std::vector<std::string> &m, const std::string &s) {
  auto ptr = std::find(m.begin(), m.end(), s);
  return std::distance(m.begin(), ptr);
}

Reducer processTransition(size_t t_i, const std::string &case_id, State result,
                          clock_s::time_point start_time,
                          clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == State::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[t_i].begin(),
                            place_list[t_i].end());
    }

    model.event_log.push_back(
        {case_id, model.net.transition[t_i], State::Started, start_time});
    model.event_log.push_back(
        {case_id, model.net.transition[t_i],
         result == State::Completed ? State::Completed : State::Error,
         end_time});

    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), t_i));
    return model;
  };
}

Reducer processTransition(size_t t_i, const Eventlog &new_events,
                          State result) {
  return [=, ev = std::move(new_events)](Model &&model) mutable -> Model & {
    if (result == State::Completed) {
      const auto &place_list = model.net.output_n;
      model.tokens_n.insert(model.tokens_n.begin(), place_list[t_i].begin(),
                            place_list[t_i].end());
    }

    model.event_log.merge(
        ev, [](const auto &a, const auto &b) { return a.stamp < b.stamp; });

    // we know for sure this transition is active because otherwise it wouldn't
    // produce a reducer.
    model.active_transitions_n.erase(
        std::find(model.active_transitions_n.begin(),
                  model.active_transitions_n.end(), t_i));

    return model;
  };
}

Reducer createReducerForTransition(size_t t_i, const PolyAction &task,
                                   const std::string &case_id) {
  const auto start_time = clock_s::now();
  const auto [ev, res] = fireTransition(task);
  const auto end_time = clock_s::now();

  return ev.empty() ? processTransition(t_i, case_id, res, start_time, end_time)
                    : processTransition(t_i, ev, res);
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

Model::Model(
    const Net &_net, const Store &store,
    const std::vector<std::pair<symmetri::Transition, int8_t>> &_priority,
    const Marking &M0)
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
          if (p == input_place && std::find(p_to_ts_n.begin(), p_to_ts_n.end(),
                                            c) == p_to_ts_n.end()) {
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

std::vector<Transition> Model::getFireableTransitions() const {
  auto possible_transition_list_n =
      possibleTransitions(tokens_n, net.p_to_ts_n, net.priority);
  std::vector<Transition> fireable_transitions;
  fireable_transitions.reserve(possible_transition_list_n.size());
  std::sort(possible_transition_list_n.begin(),
            possible_transition_list_n.end());
  std::unique(possible_transition_list_n.begin(),
              possible_transition_list_n.end());
  for (const size_t t_idx : possible_transition_list_n) {
    const auto &pre = net.input_n[t_idx];
    if (canFire(pre, tokens_n)) {
      fireable_transitions.push_back(net.transition[t_idx]);
    }
  }
  return fireable_transitions;
}

bool Model::tryFire(
    const size_t t,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers,
    const StoppablePool &pool, const std::string &case_id) {
  const auto &pre = net.input_n[t];

  if (canFire(pre, tokens_n)) {
    timestamp = clock_s::now();
    // deduct the marking
    for (const size_t place : pre) {
      // erase one by one. using std::remove_if would remove all tokens at
      // a particular place.
      tokens_n.erase(std::find(tokens_n.begin(), tokens_n.end(), place));
    }

    const auto &task = net.store[t];

    // if the transition is direct, we short-circuit the
    // marking mutation and do it immediately.
    if (isDirectTransition(task)) {
      tokens_n.insert(tokens_n.begin(), net.output_n[t].begin(),
                      net.output_n[t].end());
      event_log.push_back(
          {case_id, net.transition[t], State::Started, timestamp});
      event_log.push_back(
          {case_id, net.transition[t], State::Completed, timestamp});
    } else {
      active_transitions_n.push_back(t);
      pool.enqueue([=] {
        reducers->enqueue(createReducerForTransition(t, task, case_id));
      });
    }
    return true;
  } else {
    return false;
  }
}

bool Model::fireTransition(
    const Transition &t,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers,
    const StoppablePool &pool, const std::string &case_id) {
  auto it = std::find(net.transition.begin(), net.transition.end(), t);
  return it != net.transition.end() &&
         tryFire(std::distance(net.transition.begin(), it), reducers, pool,
                 case_id);
}

void Model::fireTransitions(
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers,
    const StoppablePool &pool, bool run_all, const std::string &case_id) {
  // find possible transitions
  auto possible_transition_list_n =
      possibleTransitions(tokens_n, net.p_to_ts_n, net.priority);
  // fire possible transitions
  for (size_t i = 0;
       i < possible_transition_list_n.size() && tokens_n.size() > 0; ++i) {
    size_t t = possible_transition_list_n[i];
    if (tryFire(t, reducers, pool, case_id)) {
      if (run_all) {
        // reset counter & update possible fire-list
        i = -1;  // minus 1 because it gets incremented by the for loop
        possible_transition_list_n =
            possibleTransitions(tokens_n, net.p_to_ts_n, net.priority);
      } else {
        break;
      }
    }
  }
  return;
}

std::pair<std::vector<Transition>, std::vector<Place>> Model::getState() const {
  return {getActiveTransitions(), getMarking()};
}

std::vector<Place> Model::getMarking() const {
  std::vector<Place> marking;
  marking.reserve(tokens_n.size());
  std::transform(tokens_n.cbegin(), tokens_n.cend(),
                 std::back_inserter(marking),
                 [&](auto place_index) { return net.place[place_index]; });
  return marking;
}

std::vector<Transition> Model::getActiveTransitions() const {
  std::vector<Transition> transition_list;
  if (active_transitions_n.size() > 0) {
    transition_list.reserve(active_transitions_n.size());
    std::transform(active_transitions_n.cbegin(), active_transitions_n.cend(),
                   std::back_inserter(transition_list), [&](auto place_index) {
                     return net.transition[place_index];
                   });
  }
  return transition_list;
}

}  // namespace symmetri
