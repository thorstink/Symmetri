#include "actions.h"

namespace symmetri {
using namespace moodycamel;

auto now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             clock_t::now().time_since_epoch())
      .count();
}

auto getThreadId() {
  return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

std::optional<Reducer> noTransitionInStore(const Transition &t) {
  return Reducer([=](Model model) {
    std::stringstream s;
    s << "Transition " << t << " is not in the store.";
    throw std::runtime_error(s.str());
    return model;
  });
}

std::optional<Reducer> markingMutation(const MarkingMutation &dM) {
  return Reducer([=](Model model) {
    model.data->M += dM;
    return model;
  });
}

std::vector<std::thread>
executeTransition(const TransitionActionMap &local_store,
                  BlockingConcurrentQueue<Reducer> &reducers,
                  BlockingConcurrentQueue<Transition> &actions) {

  unsigned int n = std::thread::hardware_concurrency();
  unsigned int thread_count = std::max<unsigned int>(1, n / 2);
  thread_count = 3;
  std::cout << thread_count << " concurrent threads are supported.\n";
  std::vector<std::thread> pool(thread_count);

  auto worker = [&] {
    Transition transition;
    while (true) {
      actions.wait_dequeue(transition);

      const auto start_time = now();
      auto fun_ptr = local_store.find(transition);
      auto model_update = (fun_ptr != local_store.end() && fun_ptr->second().has_value())
                              ? markingMutation(fun_ptr->second().value())
                              : noTransitionInStore(transition);

      const auto end_time = now();
      const auto thread_id = getThreadId();

      reducers.enqueue(
          model_update.has_value()
              ? Reducer([=, update = std::move(model_update)](
                                   Model model) mutable {
                  model.data->M += model.data->Dp.at(transition);
                  model.data->active_transitions.erase(transition);

                  model.data->log.emplace_back(std::make_tuple(
                      thread_id, start_time, end_time, transition));
                  return update.value()(model);
                })
              : Reducer([=](Model model) {
                  model.data->M += model.data->Dp.at(transition);
                  model.data->active_transitions.erase(transition);

                  model.data->log.emplace_back(std::make_tuple(
                      thread_id, start_time, end_time, transition));
                  return model;
                }));
    }
  };
  // populate the pool
  std::generate(std::begin(pool), std::end(pool),
                [worker]() { return std::thread(worker); });

  return pool;
}
} // namespace symmetri