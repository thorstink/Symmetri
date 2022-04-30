#include "model.h"

namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

Model &run_all(
    Model &model, moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<object_t> &polymorphic_actions,
    const std::string &case_id) {
  model.timestamp = clock_t::now();
  std::vector<object_t> T;
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
        const auto start_time = clock_t::now();
        run(task);
        const auto end_time = clock_t::now();

        reducers.enqueue(Reducer([=, thread_id = getThreadId()](
                                     Model &&model) -> Model & {
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
        }));
      });
      model.pending_transitions.insert(T_i);
    }
  }
  polymorphic_actions.enqueue_bulk(T.begin(), T.size());
  return model;
}
}  // namespace symmetri
