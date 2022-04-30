#include "model.h"

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

Reducer createReducerForTransitionCompletion(const std::string &T_i,
                                             const std::string &case_id,
                                             clock_t::time_point start_time,
                                             clock_t::time_point end_time) {
  return [=, thread_id = getThreadId()](Model &&model) -> Model & {
    for (const auto &m_p : model.net.at(T_i).second) {
      model.M[m_p] += 1;
    }
    model.event_log.push_back(
        {case_id, T_i, TransitionState::Started, start_time});
    model.event_log.push_back(
        {case_id, T_i, TransitionState::Completed, end_time});

    model.pending_transitions.erase(T_i);
    model.transition_end_times[T_i] = end_time;
    model.log.emplace(T_i, TaskInstance{start_time, end_time, thread_id});
    return model;
  };
}

Reducer createReducerForTransitionCompletion(const std::string &T_i,
                                             const Eventlog &el,
                                             clock_t::time_point start_time,
                                             clock_t::time_point end_time) {
  return [=, thread_id = getThreadId()](Model &&model) -> Model & {
    for (const auto &m_p : model.net.at(T_i).second) {
      model.M[m_p] += 1;
    }
    std::move(el.begin(), el.end(), std::back_inserter(model.event_log));
    model.pending_transitions.erase(T_i);
    model.transition_end_times[T_i] = end_time;
    model.log.emplace(T_i, TaskInstance{start_time, end_time, thread_id});
    return model;
  };
}

Reducer createReducerForTransitionCompletion(const std::string &T_i,
                                             const PolyAction &task,
                                             const std::string &case_id) {
  const auto start_time = clock_t::now();
  const auto &[ev, res] = run(task);
  const auto end_time = clock_t::now();
  return ev.empty() ? createReducerForTransitionCompletion(T_i, case_id,
                                                           start_time, end_time)
                    : createReducerForTransitionCompletion(T_i, ev, start_time,
                                                           end_time);
}

Model &run_all(
    Model &model, moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<PolyAction> &polymorphic_actions,
    const std::string &case_id) {
  model.timestamp = clock_t::now();
  std::vector<PolyAction> T;
  for (const auto &[T_i, mut] : model.net) {
    const auto &pre = mut.first;
    if (!pre.empty() &&
        std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
          return model.M[m_p] >= pre.count(m_p);
        })) {
      for (auto &m_p : pre) {
        model.M[m_p] -= 1;
      }
      T.push_back([&, T_i, &task = model.store.at(T_i)] {
        reducers.enqueue(
            createReducerForTransitionCompletion(T_i, task, case_id));
      });
      model.pending_transitions.insert(T_i);
    }
  }
  polymorphic_actions.enqueue_bulk(T.begin(), T.size());
  return model;
}
}  // namespace symmetri
