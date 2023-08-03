#include "symmetri/symmetri.h"

#include <functional>
#include <future>
#include <memory>
#include <optional>
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

using namespace moodycamel;

bool areAllTransitionsInStore(const Store &store, const Net &net) noexcept {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition =
        std::find_if(store.begin(), store.end(), [&](const auto &e) {
          return e.first == t;
        }) != store.end();
    if (!store_has_transition) {
      // error seriously here or default?
    }
    return store_has_transition;
  });
}

/**
 * @brief A factory function that creates a Petri and a handler that allows to
 * register triggers to functions.
 *
 * @param net
 * @param m0
 * @param final_marking
 * @param store
 * @param priority
 * @param case_id
 * @param stp
 * @return std::tuple<std::shared_ptr<Petri>, std::function<void(const
 * Transition &)>>
 */
std::tuple<std::shared_ptr<Petri>, std::function<void(const Transition &)>>
create(const Net &net, const Marking &m0, const Marking &final_marking,
       const Store &store, const PriorityTable &priority,
       const std::string &case_id, std::shared_ptr<TaskSystem> stp) {
  auto impl = std::make_shared<Petri>(net, store, priority, m0, final_marking,
                                      case_id, stp);
  return {impl, [=](const Transition &t) {
            const auto &reducer = impl->reducers[impl->reducer_selector];
            reducer->enqueue([=](Petri &&m) {
              const auto t_index = toIndex(m.net.transition, t);
              m.active_transitions_n.push_back(t_index);
              reducer->enqueue(
                  fireTransition(t_index, m.net.transition[t_index],
                                 m.net.store[t_index], m.case_id, reducer));
              return std::ref(m);
            });
          }};
}

}  // namespace symmetri

using namespace symmetri;

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : thread_id_(std::nullopt) {
  const auto [net, m0] = readPnml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const std::string &case_id, std::shared_ptr<TaskSystem> stp)
    : thread_id_(std::nullopt) {
  const auto [net, m0, priorities] = readGrml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

PetriNet::PetriNet(const Net &net, const Marking &m0,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp)
    : thread_id_(std::nullopt) {
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

std::function<void()> PetriNet::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] { register_functor(transition); };
}

bool PetriNet::reuseApplication(const std::string &new_case_id) {
  if (impl) {
    auto &petri = *impl;
    if (thread_id_.load().has_value() || new_case_id == petri.case_id) {
      return false;
    }
    petri.case_id = new_case_id;
    thread_id_.store(std::nullopt);
    return true;
  } else {
    return false;
  }
}

symmetri::Result fire(const PetriNet &app) {
  if (app.impl == nullptr || app.thread_id_.load().has_value()) {
    return {{}, symmetri::State::Error};
  }
  app.thread_id_.store(symmetri::getThreadId());

  auto &m = *app.impl;

  // we are running!
  auto &active_reducers = m.reducers[m.reducer_selector];
  m.event_log.clear();
  m.tokens_n = m.initial_tokens;
  m.state = State::Started;
  symmetri::Reducer f;
  // start!
  m.fireTransitions(active_reducers, true, m.case_id);
  while ((m.state == State::Started || m.state == State::Paused) &&
         active_reducers->wait_dequeue_timed(f, -1)) {
    do {
      m = f(std::move(m));
    } while (active_reducers->try_dequeue(f));

    m.state = symmetri::MarkingReached(m.tokens_n, m.final_marking_n)
                  ? State::Completed
                  : m.state;

    if (m.state == State::Started) {
      // we're firing
      m.fireTransitions(active_reducers, true, m.case_id);
      // if there's nothing to fire; we deadlocked
      m.state = m.active_transitions_n.size() == 0 ? State::Deadlock : m.state;
    }
  }

  // empty reducers
  m.active_transitions_n.clear();
  while (active_reducers->try_dequeue(f)) {
    m = f(std::move(m));
  }

  m.setFreshQueue();
  app.thread_id_.store(std::nullopt);
  return {m.event_log, m.state};
};

symmetri::Result cancel(const PetriNet &app) {
  app.impl->reducers[app.impl->reducer_selector]->enqueue(
      [=](symmetri::Petri &&model) {
        model.state = symmetri::State::UserExit;
        // populate that eventlog with child eventlog and possible cancelations.
        for (const auto transition_index : model.active_transitions_n) {
          auto [el, state] = cancel(model.net.store.at(transition_index));
          if (!el.empty()) {
            model.event_log.insert(model.event_log.end(), el.begin(), el.end());
          }
          model.event_log.push_back({model.case_id,
                                     model.net.transition[transition_index],
                                     state, symmetri::Clock::now()});
        }
        return std::ref(model);
      });

  return {getLog(app), symmetri::State::UserExit};
}

void pause(const PetriNet &app) {
  app.impl->reducers[app.impl->reducer_selector]->enqueue(
      [](symmetri::Petri &&model) {
        model.state = State::Paused;
        for (const auto transition_index : model.active_transitions_n) {
          pause(model.net.store.at(transition_index));
        }
        return std::ref(model);
      });
};

void resume(const PetriNet &app) {
  app.impl->reducers[app.impl->reducer_selector]->enqueue(
      [](symmetri::Petri &&model) {
        model.state = State::Started;
        for (const auto transition_index : model.active_transitions_n) {
          resume(model.net.store.at(transition_index));
        }
        return std::ref(model);
      });
};

symmetri::Eventlog getLog(const PetriNet &app) {
  if (app.thread_id_.load() && app.thread_id_.load().value() != getThreadId()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    app.impl->reducers[app.impl->reducer_selector]->enqueue([&](Petri &&model) {
      auto eventlog = model.event_log;
      // get event log from parent nets:
      for (size_t pt_idx : model.active_transitions_n) {
        auto sub_el = getLog(model.net.store[pt_idx]);
        if (!sub_el.empty()) {
          eventlog.insert(eventlog.end(), sub_el.begin(), sub_el.end());
        }
      }
      el.set_value(std::move(eventlog));
      return std::ref(model);
    });
    return el_getter.get();
  } else {
    return app.impl->event_log;
  }
}
