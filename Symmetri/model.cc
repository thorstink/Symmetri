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

void processPreConditions(const std::vector<Place> &pre_places, NetMarking &m) {
  for (const auto &m_p : pre_places) {
    m[m_p] -= 1;
  }
}

void processPostConditions(const std::vector<Place> &post_places,
                           NetMarking &m) {
  for (const auto &m_p : post_places) {
    m[m_p] += 1;
  }
}

Reducer processTransition(const std::string &T_i, const std::string &case_id,
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

Reducer processTransition(const std::string &T_i, const Eventlog &new_events,
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
                      const std::string &case_id,
                      const clock_s::time_point &queue_time) {
  const auto start_time = clock_s::now();
  // std::cout << "dt: " << (start_time - queue_time).count() << std::endl;
  const auto &[ev, res] = runTransition(task);
  const auto end_time = clock_s::now();
  const auto thread_id = getThreadId();
  return ev.empty() ? processTransition(T_i, case_id, res, thread_id,
                                        start_time, end_time)
                    : processTransition(T_i, ev, res, thread_id, end_time);
}

Model &runAll(Model &model,
              moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
              StoppablePool &polymorphic_actions, const std::string &case_id) {
  model.timestamp = clock_s::now();
  // otherwise calculate the possible transitions.
  bool last_element = false;
  do {
    for (const auto &[T_i, task] : model.store) {
      last_element = T_i == model.store.back().first;
      const auto &pre = model.net.at(T_i).first;
      if (pre.empty()) {
        continue;
      }
      if (std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
            return model.M[m_p] >=
                   std::count(std::begin(pre), std::end(pre), m_p);
          })) {
        // deduct the marking
        processPreConditions(pre, model.M);
        // if the function is nullopt_t, we short-circuit the marking
        // mutation and do it immediately.
        if constexpr (std::is_same_v<std::nullopt_t, decltype(task)>) {
          processPostConditions(model.net.at(T_i).second, model.M);
        } else {
          polymorphic_actions.enqueue(
              [&reducers, T_i, task, case_id, queue_time = clock_s::now()] {
                reducers.enqueue(runTransition(T_i, task, case_id, queue_time));
              });
          model.pending_transitions.insert(T_i);
        }
      }
    }
  } while (!last_element);

  return model;
}
}  // namespace symmetri
