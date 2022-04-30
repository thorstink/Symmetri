#include "actions.h"
namespace symmetri {

StoppablePool executeTransition(
    moodycamel::BlockingConcurrentQueue<object_t> &actions,
    unsigned int thread_count, const std::string &case_id) {
  auto stop_token = std::make_shared<std::atomic<bool>>(false);
  auto worker = [&, stop_token] {
    object_t transition;
    while (stop_token->load() == false) {
      if (actions.wait_dequeue_timed(transition,
                                     std::chrono::milliseconds(250))) {
        if (stop_token->load() == true) {
          break;
        }
        run(transition);
      }
    };
  };

  return StoppablePool(thread_count, stop_token, worker, actions);
}
}  // namespace symmetri
