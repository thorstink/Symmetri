#include "actions.h"

#include <spdlog/spdlog.h>

namespace symmetri {
using namespace moodycamel;

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

StoppablePool executeTransition(const TransitionActionMap &local_store,
                                const Conversions &marking_mapper,
                                BlockingConcurrentQueue<Reducer> &reducers,
                                BlockingConcurrentQueue<Transition> &actions,
                                int state_size, unsigned int thread_count,
                                const std::string &case_id) {
  auto stop_token = std::make_shared<std::atomic<bool>>(false);
  auto worker = [&, state_size, stop_token] {
    Transition transition;
    while (stop_token->load() == false) {
      if (actions.wait_dequeue_timed(transition,
                                     std::chrono::milliseconds(250))) {
        const auto start_time = clock_t::now();

        if (local_store.contains(transition)) {
          spdlog::get(case_id)->info("Transition {0} started.", transition);
          local_store.at(transition)();
          spdlog::get(case_id)->info("Transition {0} ended.", transition);

          const auto end_time = clock_t::now();
          const auto thread_id = getThreadId();
          reducers.enqueue(Reducer([=](Model model) {
            model.data->M += model.data->Dp.at(transition);
            model.data->active_transitions.erase(transition);
            model.data->log.emplace(
                transition, TaskInstance{start_time, end_time, thread_id});
            return model;
          }));
        } else {
          spdlog::get(case_id)->error(
              "No function assigned to transition label {0}.", transition);
        }
      }
    };
  };

  return StoppablePool(thread_count, stop_token, worker);
}
}  // namespace symmetri
