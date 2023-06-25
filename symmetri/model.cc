#include "model.h"

namespace symmetri {

std::tuple<std::vector<Transition>, std::vector<Place>, std::vector<PolyAction>>
convert(const Net &_net, const Store &_store) {
  const auto transition_count = _net.size();
  std::vector<Transition> transitions;
  std::vector<Place> places;
  std::vector<PolyAction> store;
  transitions.reserve(transition_count);
  store.reserve(transition_count);
  for (const auto &[t, io] : _net) {
    transitions.push_back(t);
    store.push_back(_store.at(t));
    for (const auto &p : io.first) {
      places.push_back(p);
    }
    for (const auto &p : io.second) {
      places.push_back(p);
    }
  }
  // sort and remove duplicates.
  std::sort(places.begin(), places.end());
  auto last = std::unique(places.begin(), places.end());
  places.erase(last, places.end());
  return {transitions, places, store};
}

Model::Model(const Net &_net, const Store &store,
             const PriorityTable &_priority, const Marking &M0)
    : timestamp(Clock::now()), event_log({}) {
  // reserve arbitrary eventlog space.
  event_log.reserve(1000);
  std::tie(net.transition, net.place, net.store) = convert(_net, store);

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
      for (size_t c = 0; c < net.transition.size(); c++) {
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

std::vector<size_t> Model::toTokens(
    const symmetri::Marking &marking) const noexcept {
  std::vector<size_t> tokens;
  for (const auto &[p, c] : marking) {
    for (int i = 0; i < c; i++) {
      tokens.push_back(toIndex(net.place, p));
    }
  }
  return tokens;
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
    std::shared_ptr<TaskSystem> pool, const std::string &case_id) {
  const auto &pre = net.input_n[t];

  if (canFire(pre, tokens_n)) {
    timestamp = Clock::now();
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
      event_log.push_back(
          {case_id, net.transition[t], State::Scheduled, timestamp});
      pool->push([=] {
        reducers->enqueue(
            createReducerForTransition(t, task, case_id, reducers));
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
    std::shared_ptr<TaskSystem> pool, const std::string &case_id) {
  auto it = std::find(net.transition.begin(), net.transition.end(), t);
  return it != net.transition.end() &&
         tryFire(std::distance(net.transition.begin(), it), reducers, pool,
                 case_id);
}

void Model::fireTransitions(
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers,
    std::shared_ptr<TaskSystem> pool, bool run_all,
    const std::string &case_id) {
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
