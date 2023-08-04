#include "symmetri/symmetri.h"

#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "model.h"
#include "symmetri/parsers.h"

namespace symmetri {

/**
 * @brief Get the Thread Id object. We go to "great lengths" to get a 32 bit
 * representation of the thread id, because in that case the atomic is lock-free
 * on (most) 64 bit systems.
 *
 * @return unsigned int
 */
unsigned int getThreadId() {
  return static_cast<unsigned int>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

void areAllTransitionsInStore(const Store &store, const Net &net) {
  for (const auto &pair : net) {
    bool store_has_transition =
        std::find_if(store.begin(), store.end(), [&](const auto &e) {
          return e.first == pair.first;
        }) != store.end();
    if (!store_has_transition) {
      std::string err = "transition " + pair.first + " is not in store";
      throw std::runtime_error(err);
    }
  }
}

}  // namespace symmetri

using namespace symmetri;

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : impl([&] {
        const auto [net, m0] = readPnml(files);
        areAllTransitionsInStore(store, net);
        return std::make_shared<Petri>(net, store, priorities, m0,
                                       final_marking, case_id, stp);
      }()) {}

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const std::string &case_id, std::shared_ptr<TaskSystem> stp)
    : impl([&] {
        const auto [net, m0, priorities] = readGrml(files);
        areAllTransitionsInStore(store, net);
        return std::make_shared<Petri>(net, store, priorities, m0,
                                       final_marking, case_id, stp);
      }()) {}

PetriNet::PetriNet(const Net &net, const Marking &m0,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : impl(std::make_shared<Petri>(net, store, priorities, m0, final_marking,
                                   case_id, stp)) {
  areAllTransitionsInStore(store, net);
}

std::function<void()> PetriNet::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] {
    if (impl->thread_id_.load()) {
      impl->active_reducers->enqueue([=](Petri &m) {
        if (m.thread_id_.load()) {
          const auto t_index = toIndex(m.net.transition, transition);
          m.active_transitions_n.push_back(t_index);
          m.active_reducers->enqueue(fireTransition(
              t_index, m.net.transition[t_index], m.net.store[t_index],
              m.case_id, m.active_reducers));
        }
      });
    }
  };
}

std::vector<Place> PetriNet::getMarking() const noexcept {
  if (impl->thread_id_.load()) {
    std::promise<std::vector<Place>> el;
    std::future<std::vector<Place>> el_getter = el.get_future();
    impl->active_reducers->enqueue(
        [&](Petri &model) { el.set_value(model.getMarking()); });
    return el_getter.get();
  } else {
    return impl->getMarking();
  }
}

bool PetriNet::reuseApplication(const std::string &new_case_id) {
  if (!impl->thread_id_.load().has_value() && new_case_id != impl->case_id) {
    impl->case_id = new_case_id;
    return true;
  }
  return false;
}

symmetri::Result fire(const PetriNet &app) {
  if (app.impl->thread_id_.load().has_value()) {
    return {{}, symmetri::State::Error};
  }
  auto &m = *app.impl;
  m.thread_id_.store(symmetri::getThreadId());

  // we are running!
  m.event_log.clear();
  m.tokens_n = m.initial_tokens;
  m.state = State::Started;
  symmetri::Reducer f;
  while (m.active_reducers->try_dequeue(f)) { /* get rid of old reducers  */
  }
  // start!
  m.fireTransitions(m.active_reducers, true, m.case_id);
  while ((m.state == State::Started || m.state == State::Paused) &&
         m.active_reducers->wait_dequeue_timed(f, -1)) {
    do {
      f(m);
    } while (m.active_reducers->try_dequeue(f));

    m.state = symmetri::MarkingReached(m.tokens_n, m.final_marking_n)
                  ? State::Completed
                  : m.state;

    if (m.state == State::Started) {
      // we're firing
      m.fireTransitions(m.active_reducers, true, m.case_id);
      // if there's nothing to fire; we deadlocked
      m.state = m.active_transitions_n.size() == 0 ? State::Deadlock : m.state;
    }
  }
  m.thread_id_.store(std::nullopt);
  // empty reducers
  m.active_transitions_n.clear();
  while (m.active_reducers->try_dequeue(f)) {
    f(m);
  }

  return {m.event_log, m.state};
}

symmetri::Result cancel(const PetriNet &app) {
  app.impl->active_reducers->enqueue([=](symmetri::Petri &model) {
    model.state = symmetri::State::UserExit;
    // populate that eventlog with child eventlog and possible
    // cancelations.
    for (const auto transition_index : model.active_transitions_n) {
      auto [el, state] = cancel(model.net.store.at(transition_index));
      if (!el.empty()) {
        model.event_log.insert(model.event_log.end(), el.begin(), el.end());
      }
      model.event_log.push_back({model.case_id,
                                 model.net.transition[transition_index], state,
                                 symmetri::Clock::now()});
    }
    model.active_transitions_n.clear();
  });

  return {getLog(app), symmetri::State::UserExit};
}

void pause(const PetriNet &app) {
  app.impl->active_reducers->enqueue([](symmetri::Petri &model) {
    model.state = State::Paused;
    for (const auto transition_index : model.active_transitions_n) {
      pause(model.net.store.at(transition_index));
    }
  });
}

void resume(const PetriNet &app) {
  app.impl->active_reducers->enqueue([](symmetri::Petri &model) {
    model.state = State::Started;
    for (const auto transition_index : model.active_transitions_n) {
      resume(model.net.store.at(transition_index));
    }
  });
}

symmetri::Eventlog getLog(const PetriNet &app) {
  if (app.impl->thread_id_.load()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    app.impl->active_reducers->enqueue(
        [&](Petri &model) { el.set_value(model.getLog()); });
    return el_getter.get();
  } else {
    return app.impl->getLog();
  }
}
