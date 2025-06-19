#include "petri.h"

namespace symmetri {
std::tuple<std::vector<std::string>, std::vector<std::string>,
           std::vector<Callback>>
convert(const Net &_net) {
  const auto transition_count = _net.size();
  std::vector<std::string> transitions;
  std::vector<std::string> places;
  std::vector<Callback> store;
  transitions.reserve(transition_count);
  store.reserve(transition_count);
  for (const auto &[t, io] : _net) {
    transitions.push_back(t);
    store.emplace_back(identity<DirectMutation>{});
    for (const auto &p : io.first) {
      places.push_back(p.first);
    }
    for (const auto &p : io.second) {
      places.push_back(p.first);
    }
    // sort and remove duplicates.
    std::sort(places.begin(), places.end());
    auto last = std::unique(places.begin(), places.end());
    places.erase(last, places.end());
  }
  return {std::move(transitions), std::move(places), std::move(store)};
}

std::tuple<std::vector<SmallVectorInput>, std::vector<SmallVectorInput>>
populateIoLookups(const Net &_net, const std::vector<Place> &ordered_places) {
  std::vector<SmallVectorInput> input_n, output_n;
  for (const auto &[t, io] : _net) {
    SmallVectorInput q_in, q_out;
    for (const auto &p : io.first) {
      q_in.push_back(
          std::make_tuple(toIndex(ordered_places, p.first), p.second));
    }
    input_n.push_back(q_in);
    for (const auto &p : io.second) {
      q_out.push_back({toIndex(ordered_places, p.first), p.second});
    }
    output_n.push_back(q_out);
  }
  return {input_n, output_n};
}

std::vector<SmallVector> createReversePlaceToTransitionLookup(
    size_t place_count, size_t transition_count,
    const std::vector<SmallVectorInput> &input_transitions) {
  std::vector<SmallVector> p_to_ts_n;
  for (size_t p = 0; p < place_count; p++) {
    SmallVector q;
    for (size_t c = 0; c < transition_count; c++) {
      for (const auto &[input_place, input_color] : input_transitions[c]) {
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

Petri::Petri(const Net &_net, const PriorityTable &_priority,
             const Marking &_initial_tokens, const Marking &_final_marking,
             const std::string &_case_id,
             std::shared_ptr<TaskSystem> threadpool)
    : log({}),
      state(Scheduled),
      case_id(_case_id),
      thread_id_(std::nullopt),
      reducer_queue(
          std::make_shared<moodycamel::BlockingConcurrentQueue<Reducer>>(128)),
      pool(threadpool) {
  log.reserve(1000);
  tokens.reserve(100);
  scheduled_callbacks.reserve(10);

  std::tie(net.transition, net.place, net.store) = convert(_net);
  std::tie(net.input_n, net.output_n) = populateIoLookups(_net, net.place);
  net.p_to_ts_n = createReversePlaceToTransitionLookup(
      net.place.size(), net.transition.size(), net.input_n);
  net.priority = createPriorityLookup(net.transition, _priority);
  initial_tokens = toTokens(_initial_tokens);
  tokens = initial_tokens;
  final_marking = toTokens(_final_marking);
}

std::vector<AugmentedToken> Petri::toTokens(
    const Marking &marking) const noexcept {
  std::vector<AugmentedToken> tokens;
  for (const auto &[p, c] : marking) {
    tokens.push_back({toIndex(net.place, p), c});
  }
  return tokens;
}

void Petri::fireSynchronous(const size_t t) {
  const auto &task = net.store[t];
  const auto &lookup_t = net.output_n[t];
  const auto now = Clock::now();
  log.push_back({t, Started, now});
  auto result = fire(task);
  log.push_back({t, result, now});
  for (const auto &[p, c] : lookup_t) {
    tokens.push_back({p, result});
  }
}

void Petri::fireAsynchronous(const size_t t_i) {
  // register that we schedule a particular transition
  scheduled_callbacks.push_back(t_i);
  log.push_back({t_i, Scheduled, Clock::now()});
  // defer execution of the transition to the threadpool
  pool->push([t_i, this] {
    // log the start on the petri loop;
    reducer_queue->enqueue([t_i, t_start = Clock::now()](Petri &model) {
      model.log.push_back({t_i, Started, t_start});
    });

    // fire the transition and defer a reducer to the petri loop to update the
    // marking and log
    reducer_queue->enqueue([t_i, result = fire(net.store[t_i])](Petri &model) {
      // if it is in the active transition set it means it is finished and
      // we should process it.
      const auto t_end = model.net.store[t_i].getEndTime();
      const auto it = std::find(model.scheduled_callbacks.begin(),
                                model.scheduled_callbacks.end(), t_i);
      if (it != model.scheduled_callbacks.end()) {
        const auto &place_list = model.net.output_n[t_i];
        if (model.tokens.size() + place_list.size() > model.tokens.capacity()) {
          model.tokens.reserve(
              std::max(2 * model.tokens.size(),
                       model.tokens.size() + place_list.size()));
        }
        for (const auto &[p, c] : place_list) {
          model.tokens.emplace_back(p, result);
        }
        std::swap(*std::prev(model.scheduled_callbacks.end()), *it);
        model.scheduled_callbacks.pop_back();
      };
      model.log.push_back({t_i, result, t_end});
    });
  });
}

void deductMarking(std::vector<AugmentedToken> &tokens,
                   const SmallVectorInput &inputs) {
  for (const auto &place : inputs) {
    tokens.erase(std::find(tokens.begin(), tokens.end(), place));
  }
}

void Petri::fireTransitions() {
  auto ts = possibleTransitions(tokens, net.input_n, net.p_to_ts_n);
  std::sort(ts.begin(), ts.end(), [&](size_t a, size_t b) {
    return net.priority[a] > net.priority[b];
  });

  // loop
  while (!ts.empty()) {
    const auto t_idx = ts.front();
    const bool is_synchronous = isSynchronous(net.store[t_idx]);
    const bool can_fire = canFire(net.input_n[t_idx], tokens);

    // fire!
    if (can_fire) {
      deductMarking(tokens, net.input_n[t_idx]);
      std::invoke(
          is_synchronous ? &Petri::fireSynchronous : &Petri::fireAsynchronous,
          this, t_idx);
    }

    // If the transition couldn't be fired or can not be fired again, remove the
    // transition
    if (not can_fire || not canFire(net.input_n[t_idx], tokens)) {
      std::rotate(ts.begin(), ts.begin() + 1, ts.end());
      ts.pop_back();
    }

    // if the fired transition was synchronous, we have to check if the new
    // tokens enable possible transitions
    if (can_fire && is_synchronous) {
      for (const auto &[p, c] : net.output_n[t_idx]) {
        for (const auto &new_transition : net.p_to_ts_n[p]) {
          if (std::find(ts.begin(), ts.end(), new_transition) == ts.end()) {
            ts.push_back(new_transition);
          }
        }
      }
      // we have to sort by priority again because we added priorities.
      std::sort(ts.begin(), ts.end(), [&](size_t a, size_t b) {
        return net.priority[a] > net.priority[b];
      });
    }
  }

  return;
}

Marking Petri::getMarking() const {
  Marking marking;
  marking.reserve(tokens.size());
  std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(marking),
                 [&](auto place_index) -> std::pair<std::string, Token> {
                   return {net.place[std::get<size_t>(place_index)],
                           std::get<Token>(place_index)};
                 });
  return marking;
}

Eventlog Petri::getLogInternal() const {
  Eventlog eventlog;
  eventlog.reserve(log.size());
  for (const auto &[t_i, result, time] : log) {
    eventlog.push_back({case_id, net.transition[t_i], result, time});
  }

  // get event log from parent nets:
  for (const auto &callback : net.store) {
    Eventlog sub_el = getLog(callback);
    if (!sub_el.empty()) {
      eventlog.insert(eventlog.end(), sub_el.begin(), sub_el.end());
    }
  }

  std::sort(eventlog.begin(), eventlog.end(),
            [](const auto &a, const auto &b) { return a.stamp < b.stamp; });

  return eventlog;
}

}  // namespace symmetri
