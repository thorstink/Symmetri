#include "symmetri/symmetri.h"

#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "petri.h"
#include "symmetri/parsers.h"
#include "symmetri/utilities.hpp"

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

}  // namespace symmetri

using namespace symmetri;

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : impl([&] {
        const auto [net, m0] = readPnml(files);
        return std::make_shared<Petri>(net, store, priorities, m0,
                                       final_marking, case_id, stp);
      }()) {}

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const std::string &case_id, std::shared_ptr<TaskSystem> stp)
    : impl([&] {
        const auto [net, m0, priorities] = readGrml(files);
        return std::make_shared<Petri>(net, store, priorities, m0,
                                       final_marking, case_id, stp);
      }()) {}

PetriNet::PetriNet(const Net &net, const Marking &m0,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : impl(std::make_shared<Petri>(net, store, priorities, m0, final_marking,
                                   case_id, stp)) {}

std::function<void()> PetriNet::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] {
    if (impl->thread_id_.load()) {
      impl->reducer_queue->enqueue([=](Petri &m) {
        if (m.thread_id_.load()) {
          const auto t_index = toIndex(m.net.transition, transition);
          m.scheduled_callbacks.push_back(t_index);
          m.reducer_queue->enqueue(
              scheduleCallback(t_index, m.net.store[t_index], m.reducer_queue));
        }
      });
    }
  };
}

Marking PetriNet::getMarking() const noexcept {
  if (impl->thread_id_.load()) {
    std::promise<Marking> el;
    std::future<Marking> el_getter = el.get_future();
    impl->reducer_queue->enqueue(
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

symmetri::Token fire(const PetriNet &app) {
  if (app.impl->thread_id_.load().has_value()) {
    return symmetri::state::Error;
  }
  auto &m = *app.impl;
  m.thread_id_.store(symmetri::getThreadId());

  // we are running!
  m.event_log.clear();
  m.scheduled_callbacks.clear();
  m.tokens = m.initial_tokens;
  m.state = state::Started;
  symmetri::Reducer f;
  while (m.reducer_queue->try_dequeue(f)) { /* get rid of old reducers  */
  }
  // start!
  m.fireTransitions();
  while ((m.state == state::Started || m.state == state::Paused) &&
         m.reducer_queue->wait_dequeue_timed(f, -1)) {
    do {
      f(m);
    } while (m.reducer_queue->try_dequeue(f));

    if (symmetri::MarkingReached(m.tokens, m.final_marking)) {
      m.state = state::Completed;
    }

    if (m.state == state::Started) {
      // we're firing
      m.fireTransitions();
      // if there's nothing to fire; we deadlocked
      if (m.scheduled_callbacks.size() == 0) {
        m.state = state::Deadlock;
      }
    }
  }

  if (symmetri::MarkingReached(m.tokens, m.final_marking)) {
    m.state = state::Completed;
  }

  while (!m.scheduled_callbacks.empty()) {
    if (m.reducer_queue->wait_dequeue_timed(f, std::chrono::milliseconds(10))) {
      f(m);
    }
  }

  m.thread_id_.store(std::nullopt);

  return m.state;
}

void cancel(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([=](symmetri::Petri &model) {
    model.state = symmetri::state::UserExit;
    // populate that eventlog with child eventlog and possible
    // cancelations.
    for (const auto transition_index : model.scheduled_callbacks) {
      cancel(model.net.store.at(transition_index));
      model.event_log.push_back({model.case_id,
                                 model.net.transition[transition_index],
                                 state::UserExit, symmetri::Clock::now()});
    }
  });
}

void pause(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](symmetri::Petri &model) {
    model.state = state::Paused;
    for (const auto transition_index : model.scheduled_callbacks) {
      pause(model.net.store.at(transition_index));
    }
  });
}

void resume(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](symmetri::Petri &model) {
    model.state = state::Started;
    for (const auto transition_index : model.scheduled_callbacks) {
      resume(model.net.store.at(transition_index));
    }
  });
}

symmetri::Eventlog getLog(const PetriNet &app) {
  if (app.impl->thread_id_.load()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    app.impl->reducer_queue->enqueue(
        [&](Petri &model) { el.set_value(model.getLog()); });
    return el_getter.get();
  } else {
    return app.impl->getLog();
  }
}
