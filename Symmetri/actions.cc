#include "actions.h"
#include "Symmetri/expected.hpp"

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

tl::expected<std::function<OptionalError()>, std::string>
getAction(const TransitionActionMap &local_store, const Transition &t) {
  auto fun_ptr = local_store.find(t);
  if (fun_ptr != local_store.end()) {
    return fun_ptr->second;
  } else {
    return tl::make_unexpected("noop");
  }
}

OptionalError executeAction(std::function<OptionalError()> action) {
  return action();
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

      auto t = getAction(local_store, transition);

      if (t.has_value()) {
        auto optional_error = t.value()();
        const auto end_time = now();
        const auto thread_id = getThreadId();
        if (optional_error.has_value()) {
          MarkingMutation error_mutation = optional_error.value();
          reducers.enqueue(Reducer([=](Model model) {
            model.data->M += error_mutation;
            model.data->active_transitions.erase(transition);
            model.data->log.emplace_back(
                std::make_tuple(thread_id, start_time, end_time, transition));
            return model;
          }));
        } else {
          reducers.enqueue(Reducer([=](Model model) {
            model.data->M += model.data->Dp.at(transition);
            model.data->active_transitions.erase(transition);
            model.data->log.emplace_back(
                std::make_tuple(thread_id, start_time, end_time, transition));
            return model;
          }));
        }
      }
    };
  };
  // populate the pool
  std::generate(std::begin(pool), std::end(pool),
                [worker]() { return std::thread(worker); });

  return pool;
}
} // namespace symmetri