#include "actions.h"

namespace actions {
using namespace rxcpp;

auto now() {
  return std::chrono::steady_clock::now().time_since_epoch().count();
}

auto getThreadId() {
  return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

auto noTransitionInStore(const types::Transition &t) {
  return model::Reducer([=](model::Model model) {
    std::stringstream s;
    s << "Transition " << t << " is not in the store.";
    throw std::runtime_error(s.str());
    return model;
  });
}

r_type executeTransition(const observe_on_one_worker &scheduler,
                         const model::TransitionActionMap &local_store) {
  return [=](const types::Transition &transition) {
    return observable<>::just(transition, scheduler) |
           operators::map([local_store](
                              const types::Transition &transition) {
             const auto start_time = now();
             auto fun_ptr = local_store.find(transition);
             auto model_update = (fun_ptr != local_store.end())
                                     ? fun_ptr->second()
                                     : noTransitionInStore(transition);
             const auto end_time = now();
             const auto thread_id = getThreadId();
             return model_update.has_value()
                        ? model::Reducer([=, update = *model_update](
                                             model::Model model) {
                            model.data->M += model.data->Dp.at(transition);
                            model.data->log.emplace_back(std::make_tuple(
                                thread_id, start_time, end_time, transition));
                            return update(model);
                          })
                        : model::Reducer([=](model::Model model) {
                            model.data->M += model.data->Dp.at(transition);
                            model.data->log.emplace_back(std::make_tuple(
                                thread_id, start_time, end_time, transition));
                            return model;
                          });
           });
  };
}
} // namespace actions
