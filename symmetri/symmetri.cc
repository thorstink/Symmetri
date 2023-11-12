#include "symmetri/symmetri.h"

#include <filesystem>
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

PetriNet::PetriNet(const std::set<std::string> &files,
                   const std::string &case_id,
                   std::shared_ptr<TaskSystem> threadpool,
                   const Marking &final_marking,
                   const PriorityTable &priorities)
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
      }()) {}

PetriNet::PetriNet(const Net &net, const std::string &case_id,
                   std::shared_ptr<TaskSystem> threadpool,
                   const Marking &initial_marking, const Marking &final_marking,
                   const PriorityTable &priorities)
    : impl(std::make_shared<Petri>(net, priorities, initial_marking,
                                   final_marking, case_id, threadpool)) {}

std::function<void()> PetriNet::getInputTransitionHandle(
    const Transition &transition) const noexcept {
  const auto t_index = toIndex(impl->net.transition, transition);
  // if the transition has input places, you can not register a callback like
  // this, we simply return a non-functioning handle.
  if (!impl->net.input_n[t_index].empty()) {
    return []() -> void {};
  } else {
    return [t_index, this]() -> void {
      if (impl->thread_id_.load()) {
        impl->reducer_queue->enqueue([=](Petri &m) {
          m.scheduled_callbacks.push_back(t_index);
          m.reducer_queue->enqueue(
              scheduleCallback(t_index, m.net.store[t_index], m.reducer_queue));
        });
      }
    };
  }
}

void PetriNet::registerCallback(
    const std::string &transition,
    const symmetri::Callback &callback) const noexcept {
  if (!impl->thread_id_.load().has_value()) {
    impl->net.registerCallback(transition, callback);
  }
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
    return symmetri::Color::Failed;
  }
  auto &m = *app.impl;
  m.thread_id_.store(symmetri::getThreadId());
  m.scheduled_callbacks.clear();
  m.tokens = m.initial_tokens;
  m.state = Color::Started;
  symmetri::Reducer f;
  while (m.reducer_queue->try_dequeue(f)) { /* get rid of old reducers  */
  }

  // start!
  m.reducer_queue->enqueue([=](Petri &) {});
  while ((m.state == Color::Started || m.state == Color::Paused) &&
         m.reducer_queue->wait_dequeue_timed(f, -1)) {
    do {
      f(m);
    } while (m.reducer_queue->try_dequeue(f));

    if (symmetri::MarkingReached(m.tokens, m.final_marking)) {
      m.state = Color::Success;
    }

    if (m.state == Color::Started) {
      // we're firing
      m.fireTransitions();
      // if there's nothing to fire; we deadlocked
      if (m.scheduled_callbacks.size() == 0) {
        m.state = Color::Deadlocked;
      } else if (symmetri::MarkingReached(m.tokens, m.final_marking)) {
        m.state = Color::Success;
      }
    }
  }

  if (symmetri::MarkingReached(m.tokens, m.final_marking)) {
    m.state = Color::Success;
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
  app.impl->reducer_queue->enqueue([=](Petri &model) {
    model.state = Color::Canceled;
    for (const auto transition_index : model.scheduled_callbacks) {
      cancel(model.net.store.at(transition_index));
      model.log.push_back({transition_index, Color::Canceled, Clock::now()});
    }
  });
}

void pause(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](Petri &model) {
    model.state = Color::Paused;
    for (const auto transition_index : model.scheduled_callbacks) {
      pause(model.net.store.at(transition_index));
    }
  });
}

void resume(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](Petri &model) {
    model.state = Color::Started;
    for (const auto transition_index : model.scheduled_callbacks) {
      resume(model.net.store.at(transition_index));
    }
  });
}

Eventlog getLog(const PetriNet &app) {
  if (app.impl->thread_id_.load()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    app.impl->reducer_queue->enqueue(
        [&](Petri &model) { el.set_value(model.getLogInternal()); });
    return el_getter.get();
  } else {
    return app.impl->getLogInternal();
  }
}

}  // namespace symmetri
