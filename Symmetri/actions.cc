#include "actions.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace symmetri {
using namespace moodycamel;
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

StoppablePool executeTransition(const TransitionActionMap &local_store,
                                BlockingConcurrentQueue<Reducer> &reducers,
                                BlockingConcurrentQueue<Transition> &actions,
                                unsigned int thread_count,
                                const std::string &case_id) {
  auto stop_token = std::make_shared<std::atomic<bool>>(false);
  auto worker = [&, stop_token] {
    Transition transition;
    while (stop_token->load() == false) {
      if (actions.wait_dequeue_timed(transition,
                                     std::chrono::milliseconds(250))) {
        if (stop_token->load() == true) {
          break;
        }
        const auto start_time = clock_t::now();
        reducers.enqueue(Reducer([=](Model &&model) {
          model.data->event_log.push_back(
              {case_id, transition, TransitionState::Started, start_time});
          return model;
        }));

        std::vector<Event> event_log;
        std::visit(overloaded{
                       [](const std::function<void()> &arg) { arg(); },
                       [&event_log](const Application &arg) {
                         event_log = arg();

                         spdlog::info(
                             "BREAK MOI {0}",
                             clock_t::now().time_since_epoch().count());
                       },
                   },
                   local_store.at(transition));

        const auto end_time = clock_t::now();

        const auto thread_id = getThreadId();
        reducers.enqueue(Reducer([=](Model &&model) {
          for (const auto &m_p : model.data->net.at(transition).second) {
            model.data->M[m_p] += 1;
          }

          model.data->pending_transitions.erase(transition);
          model.data->trace.push_back(transition);
          model.data->transition_end_times[transition] = end_time;
          model.data->log.emplace(
              transition, TaskInstance{start_time, end_time, thread_id});
          if (!event_log.empty()) {
            std::move(event_log.begin(), event_log.end(),
                      std::back_inserter(model.data->event_log));
          }
          model.data->event_log.push_back(
              {case_id, transition, TransitionState::Completed, end_time});

          return model;
        }));
      }
    };
  };

  return StoppablePool(thread_count, stop_token, worker, actions);
}
}  // namespace symmetri
