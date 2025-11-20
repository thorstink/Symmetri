#include "symmetri/symmetri.h"

#include <filesystem>
#include <future>

#include "petri.h"
#include "symmetri/parsers.h"

namespace symmetri {

PetriNet::PetriNet(const std::set<std::string>& files,
                   const std::string& case_id,
                   std::shared_ptr<TaskSystem> threadpool,
                   const Marking& final_marking,
                   const PriorityTable& priorities)
    : impl([&] {
        // get the first file;
        const std::filesystem::path pn_file = *files.begin();
        if (pn_file.extension() == ".pnml") {
          const auto [net, m0] = readPnml(files);
          return std::make_shared<Petri>(net, priorities, m0, final_marking,
                                         case_id, threadpool);
        } else {
          const auto [net, m0, specific_priorities] = readGrml(files);
          return std::make_shared<Petri>(net, specific_priorities, m0,
                                         final_marking, case_id, threadpool);
        }
      }()),
      s(impl->net.store) {}

PetriNet::PetriNet(const Net& net, const std::string& case_id,
                   std::shared_ptr<TaskSystem> threadpool,
                   const Marking& initial_marking, const Marking& final_marking,
                   const PriorityTable& priorities)
    : impl(std::make_shared<Petri>(net, priorities, initial_marking,
                                   final_marking, case_id, threadpool)),
      s(impl->net.store) {}

std::function<void()> PetriNet::getInputTransitionHandle(
    const Transition& transition) const noexcept {
  const auto t_index = toIndex(impl->net.transition, transition);
  // if the transition has input places, you can not register a callback like
  // this, we simply return a non-functioning handle.
  if (!impl->net.input_n[t_index].empty()) {
    return []() -> void {
      // adding an error message here might be kind to do
    };
  } else {
    return [t_index, this]() -> void {
      if (impl->thread_id_.load()) {
        impl->reducer_queue->enqueue(
            [t_index](Petri& m) { m.fireAsynchronous(t_index); });
      }
    };
  }
}

void PetriNet::registerCallback(const std::string& transition,
                                Callback&& callback) const noexcept {
  if (!impl->thread_id_.load().has_value()) {
    impl->net.registerCallback(transition, std::forward<Callback>(callback));
  }
}
std::vector<Callback>::iterator PetriNet::getCallbackItr(
    const std::string& transition_name) const {
  const auto& t = impl->net.transition;
  return impl->net.store.begin() +
         std::distance(t.begin(),
                       std::find(t.begin(), t.end(), transition_name));
}

Marking PetriNet::getMarking() const noexcept {
  if (impl->thread_id_.load()) {
    std::promise<Marking> el;
    std::future<Marking> el_getter = el.get_future();
    impl->reducer_queue->enqueue(
        [&](Petri& model) { el.set_value(model.getMarking()); });
    return el_getter.get();
  } else {
    return impl->getMarking();
  }
}

std::vector<Transition> PetriNet::getActiveTransitions() const noexcept {
  if (impl->thread_id_.load()) {
    std::promise<std::vector<Transition>> el;
    std::future<std::vector<Transition>> el_getter = el.get_future();
    impl->reducer_queue->enqueue(
        [&](Petri& model) { el.set_value(model.getActiveTransitions()); });
    return el_getter.get();
  } else {
    return impl->getActiveTransitions();
  }
}

bool PetriNet::reuseApplication(const std::string& new_case_id) {
  if (!impl->thread_id_.load().has_value() && new_case_id != impl->case_id) {
    impl->case_id = new_case_id;
    return true;
  }
  return false;
}

}  // namespace symmetri
