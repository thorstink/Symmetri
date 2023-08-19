#include "petri.h"

bool isSynchronous(const DirectMutation &) { return true; }

symmetri::Result fire(const DirectMutation &) {
  return {{}, symmetri::State::Completed};
}

namespace symmetri {
std::tuple<std::vector<Transition>, std::vector<Place>, std::vector<Callback>>
convert(const Net &_net, const Store &_store) {
  const auto transition_count = _net.size();
  std::vector<Transition> transitions;
  std::vector<Place> places;
  std::vector<Callback> store;
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

Petri::Petri(const Net &_net, const Store &_store,
             const PriorityTable &_priority, const Marking &_initial_tokens,
             const Marking &_final_marking, const std::string &_case_id,
             std::shared_ptr<TaskSystem> stp)
    : event_log({}),
      state(State::Scheduled),
      case_id(_case_id),
      thread_id_(std::nullopt),
      reducer_queue(
          std::make_shared<moodycamel::BlockingConcurrentQueue<Reducer>>(128)),
      pool(stp) {
  event_log.reserve(1000);
  std::tie(net.transition, net.place, net.store) = convert(_net, _store);
  std::tie(net.input_n, net.output_n) = populateIoLookups(_net, net.place);
  net.p_to_ts_n = createReversePlaceToTransitionLookup(
      net.place.size(), net.transition.size(), net.input_n);
  net.priority = createPriorityLookup(net.transition, _priority);
  initial_tokens = toTokens(_initial_tokens);
  tokens = initial_tokens;
  final_marking = toTokens(_final_marking);
}

std::vector<size_t> Petri::toTokens(const Marking &marking) const noexcept {
  std::vector<size_t> tokens;
  for (const auto &[p, c] : marking) {
    for (int i = 0; i < c; i++) {
      tokens.push_back(toIndex(net.place, p));
    }
  }
  return tokens;
}

void Petri::fireSynchronous(const size_t t) {
  const auto timestamp = Clock::now();
  const auto &task = net.store[t];
  const auto &transition = net.transition[t];
  const auto &lookup_t = net.output_n[t];
  event_log.push_back({case_id, transition, State::Started, timestamp});
  auto result = std::get<symmetri::State>(::fire(task));
  event_log.push_back({case_id, transition, result, Clock::now()});
  if (result == State::Completed) {
    tokens.insert(tokens.begin(), lookup_t.begin(), lookup_t.end());
  }
}

void Petri::fireAsynchronous(const size_t t) {
  const auto timestamp = Clock::now();
  const auto &task = net.store[t];
  const auto &transition = net.transition[t];
  active_transitions.push_back(t);
  event_log.push_back({case_id, transition, State::Scheduled, timestamp});
  pool->push([=] {
    reducer_queue->enqueue(
        scheduleCallback(t, transition, task, case_id, reducer_queue));
  });
}

void Petri::deductMarking(const SmallVector &inputs) {
  for (const size_t place : inputs) {
    // erase one by one. using std::remove_if would remove all tokens at
    // a particular place.
    tokens.erase(std::find(tokens.begin(), tokens.end(), place));
  }
}

void Petri::tryFire(const Transition &t) {
  auto it = std::find(net.transition.begin(), net.transition.end(), t);
  const auto t_idx = std::distance(net.transition.begin(), it);
  if (canFire(net.input_n[t_idx], tokens)) {
    deductMarking(net.input_n[t_idx]);
    isSynchronous(net.store[t_idx]) ? fireSynchronous(t_idx)
                                    : fireAsynchronous(t_idx);
  }
}

void Petri::fireTransitions() {
  // find possible transitions
  auto possible_transition_list_n = possibleTransitions(tokens, net.p_to_ts_n);
  auto remove_inactive_transitions_predicate = [&](const auto &tokens) {
    possible_transition_list_n.erase(
        std::remove_if(
            possible_transition_list_n.begin(),
            possible_transition_list_n.end(),
            [&](auto t_idx) { return !canFire(net.input_n[t_idx], tokens); }),
        possible_transition_list_n.end());
  };

  // remove transitions that are not active;
  remove_inactive_transitions_predicate(tokens);

  // sort transition list according to priority
  std::sort(
      possible_transition_list_n.begin(), possible_transition_list_n.end(),
      [&](size_t a, size_t b) { return net.priority[a] > net.priority[b]; });

  while (!possible_transition_list_n.empty()) {
    const auto t_idx = possible_transition_list_n.front();
    deductMarking(net.input_n[t_idx]);
    if (isSynchronous(net.store[t_idx])) {
      fireSynchronous(t_idx);
      // add the output places-connected transitions as new possible
      // transitions:
      for (const auto &p : net.output_n[t_idx]) {
        for (const auto t : net.p_to_ts_n[p]) {
          if (canFire(net.input_n[t], tokens)) {
            possible_transition_list_n.push_back(t);
          }
        }
      }

      // sort again
      std::sort(possible_transition_list_n.begin(),
                possible_transition_list_n.end(), [&](size_t a, size_t b) {
                  return net.priority[a] > net.priority[b];
                });
    } else {
      fireAsynchronous(t_idx);
    }
    // remove transitions that are not active anymore because of the marking
    // mutation;
    remove_inactive_transitions_predicate(tokens);
  }

  return;
}

std::vector<Place> Petri::getMarking() const {
  std::vector<Place> marking;
  marking.reserve(tokens.size());
  std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(marking),
                 [&](auto place_index) { return net.place[place_index]; });
  return marking;
}

Eventlog Petri::getLog() const {
  Eventlog eventlog = event_log;
  // get event log from parent nets:
  for (size_t pt_idx : active_transitions) {
    auto sub_el = ::getLog(net.store[pt_idx]);
    if (!sub_el.empty()) {
      eventlog.insert(eventlog.end(), sub_el.begin(), sub_el.end());
    }
  }
  return eventlog;
}

}  // namespace symmetri
