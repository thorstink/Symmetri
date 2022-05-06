#include "model.h"

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

Reducer runTransition(const std::string &T_i, const std::string &case_id,
                      TransitionState result, size_t thread_id,
                      clock_s::time_point start_time,
                      clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      for (const auto &m_p : model.net.at(T_i).second) {
        model.M[m_p] += 1;
      }
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

Reducer runTransition(const std::string &T_i, const Eventlog &el,
                      TransitionState result, size_t thread_id,
                      clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      for (const auto &m_p : model.net.at(T_i).second) {
        model.M[m_p] += 1;
      }
    }

    std::move(el.begin(), el.end(), std::back_inserter(model.event_log));
    model.pending_transitions.erase(T_i);
    return model;
  };
}

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id) {
  const auto start_time = clock_s::now();
  const auto &[ev, res] = run(task);
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
  std::set<std::string> new_pending_transitions;
  const auto marking_hash = hashNM(model.M);
  if (model.cache.contains(marking_hash)) {
    std::tie(model.M, T, new_pending_transitions) = model.cache[marking_hash];
  } else {
    for (const auto &[T_i, mut] : model.net) {
      const auto &pre = mut.first;
      if (!pre.empty() &&
          std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
            return model.M[m_p] >= pre.count(m_p);
          })) {
        for (auto &m_p : pre) {
          model.M[m_p] -= 1;
        }
        T.push_back([&, T_i, case_id, &task = model.store.at(T_i)] {
          reducers.enqueue(runTransition(T_i, task, case_id));
        });
        new_pending_transitions.insert(T_i);
      }
    }
    model.cache[marking_hash] = {model.M, T, new_pending_transitions};
  }

  std::move(new_pending_transitions.begin(), new_pending_transitions.end(),
            std::inserter(model.pending_transitions,
                          model.pending_transitions.end()));

  polymorphic_actions.enqueue_bulk(T.begin(), T.size());

  return model;
}
}  // namespace symmetri
