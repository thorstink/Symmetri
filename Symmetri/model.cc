#include "model.h"

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

inline void processPreConditions(const std::vector<Place> &pre_places,
                                 NetMarking &m) {
  for (const auto &m_p : pre_places) {
    m[m_p] -= 1;
  }
}

inline void processPostConditions(const std::vector<Place> &post_places,
                                  NetMarking &m) {
  for (const auto &m_p : post_places) {
    m[m_p] += 1;
  }
}

Reducer runTransition(const std::string &T_i, const std::string &case_id,
                      TransitionState result, size_t thread_id,
                      clock_s::time_point start_time,
                      clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      processPostConditions(model.net.at(T_i).second, model.M);
    }
    model.event_log.push_back(
        {case_id, T_i, TransitionState::Started, start_time, thread_id});
    model.event_log.push_back({case_id, T_i,
                               result == TransitionState::Completed
                                   ? TransitionState::Completed
                                   : TransitionState::Error,
                               end_time, thread_id});

    model.pending_transitions.erase(T_i);
    return model;
  };
}

Reducer runTransition(const std::string &T_i, const Eventlog &new_events,
                      TransitionState result, size_t thread_id,
                      clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      processPostConditions(model.net.at(T_i).second, model.M);
    }

    for (const auto &e : new_events) {
      model.event_log.push_back(e);
    }
    model.pending_transitions.erase(T_i);
    return model;
  };
}

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id) {
  const auto start_time = clock_s::now();
  const auto &[ev, res] = runTransition(task);
  const auto end_time = clock_s::now();
  const auto thread_id = getThreadId();
  return ev.empty()
             ? runTransition(T_i, case_id, res, thread_id, start_time, end_time)
             : runTransition(T_i, ev, res, thread_id, end_time);
}

size_t hashNM(const NetMarking &M) {
  size_t seed = 0;
  for (const auto &place : M) {
    seed ^= std::hash<uint16_t>{}(place.second) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
  }
  return seed;
}

Model &runAll(
    Model &model, moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<PolyAction> &polymorphic_actions,
    const std::string &case_id) {
  model.timestamp = clock_s::now();
  std::vector<PolyAction> T;
  // otherwise calculate the possible transitions.
  for (const auto &[T_i, mut] : model.net) {
    const auto &pre = mut.first;
    if (!pre.empty() &&
        std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
          auto count = std::count(std::begin(pre), std::end(pre), m_p);
          return model.M[m_p] >= count;
        })) {
      // deduct the marking
      processPreConditions(pre, model.M);
      // if the function is nullopt_t, we short-circuit the marking
      // mutation and do it immediately.
      if constexpr (std::is_same_v<std::nullopt_t,
                                   decltype(model.store.at(T_i))>) {
        processPostConditions(model.net.at(T_i).second, model.M);
      } else {
        T.push_back([&, T_i, case_id, &task = model.store.at(T_i)] {
          reducers.enqueue(runTransition(T_i, task, case_id));
        });
      }
      model.pending_transitions.insert(T_i);
    }
  }

  polymorphic_actions.enqueue_bulk(T.begin(), T.size());

  return model;
}
}  // namespace symmetri
