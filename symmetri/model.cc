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
    // sort and remove duplicates.
    std::sort(places.begin(), places.end());
    auto last = std::unique(places.begin(), places.end());
    places.erase(last, places.end());
  }
  return {transitions, places, store};
}

std::tuple<std::vector<symmetri::SmallVector>,
           std::vector<symmetri::SmallVector>>
populateIoLookups(const Net &_net, const std::vector<Place> &ordered_places) {
  std::vector<symmetri::SmallVector> input_n, output_n;
  for (const auto &[t, io] : _net) {
    SmallVector q;
    for (const auto &p : io.first) {
      q.push_back(toIndex(ordered_places, p));
    }
    input_n.push_back(q);
    q.clear();
    for (const auto &p : io.second) {
      q.push_back(toIndex(ordered_places, p));
    }
    output_n.push_back(q);
  }
  return {input_n, output_n};
}

std::vector<symmetri::SmallVector> createReversePlaceToTransitionLookup(
    size_t place_count, size_t transition_count,
    const std::vector<symmetri::SmallVector> &input_transitions) {
  std::vector<symmetri::SmallVector> p_to_ts_n;
  for (size_t p = 0; p < place_count; p++) {
    SmallVector q;
    for (size_t c = 0; c < transition_count; c++) {
      for (const auto &input_place : input_transitions[c]) {
        if (p == input_place && std::find(q.begin(), q.end(), c) == q.end()) {
          q.push_back(c);
        }
      }
    }
    p_to_ts_n.push_back(q);
  }
  return p_to_ts_n;
}

std::vector<int8_t> createPriorityLookup(
    const std::vector<Transition> transition, const PriorityTable &_priority) {
  std::vector<int8_t> priority;
  for (const auto &t : transition) {
    auto ptr = std::find_if(_priority.begin(), _priority.end(),
                            [t](const auto &a) { return a.first == t; });
    priority.push_back(ptr != _priority.end() ? ptr->second : 0);
  }
  return priority;
}

Model::Model(const Net &_net, const Store &store,
             const PriorityTable &_priority, const Marking &M0)
    : timestamp(Clock::now()), event_log({}) {
  // reserve arbitrary eventlog space.
  event_log.reserve(1000);
  std::tie(net.transition, net.place, net.store) = convert(_net, store);
  std::tie(net.input_n, net.output_n) = populateIoLookups(_net, net.place);
  net.p_to_ts_n = createReversePlaceToTransitionLookup(
      net.place.size(), net.transition.size(), net.input_n);
  net.priority = createPriorityLookup(net.transition, _priority);
  initial_tokens = toTokens(M0);
  tokens_n = initial_tokens;
}

std::vector<size_t> Model::toTokens(const Marking &marking) const noexcept {
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
  for (const size_t t_idx : possible_transition_list_n) {
    const auto &pre = net.input_n[t_idx];
    if (canFire(pre, tokens_n)) {
      fireable_transitions.push_back(net.transition[t_idx]);
    }
  }
  return fireable_transitions;
}

bool Model::Fire(
    const size_t t,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers,
    std::shared_ptr<TaskSystem> pool, const std::string &case_id) {
  const auto &pre = net.input_n[t];

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
    return true;
  } else {
    active_transitions_n.push_back(t);
    event_log.push_back(
        {case_id, net.transition[t], State::Scheduled, timestamp});
    pool->push([=] {
      reducers->enqueue(createReducerForTransition(t, task, case_id, reducers));
    });
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
         canFire(net.input_n[std::distance(net.transition.begin(), it)],
                 tokens_n) &&
         !Fire(std::distance(net.transition.begin(), it), reducers, pool,
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
  auto filter_transitions = [&](const auto &tokens) {
    possible_transition_list_n.erase(
        std::remove_if(
            possible_transition_list_n.begin(),
            possible_transition_list_n.end(),
            [&](auto t_idx) { return !canFire(net.input_n[t_idx], tokens); }),
        possible_transition_list_n.end());
  };

  // remove transitions that are not fireable;
  filter_transitions(tokens_n);

  if (possible_transition_list_n.empty()) {
    return;
  }

  do {
    // if Fire returns true, update the possible transition list
    if (Fire(possible_transition_list_n.front(), reducers, pool, case_id)) {
      possible_transition_list_n =
          possibleTransitions(tokens_n, net.p_to_ts_n, net.priority);
    }
    // remove transitions that are not fireable;
    filter_transitions(tokens_n);
  } while (run_all && !possible_transition_list_n.empty());

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
