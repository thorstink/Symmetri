#include "symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <tuple>

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

Result fireTransition(const Application &app) { return app.execute(); };

Result cancelTransition(const Application &app) { return app.exitEarly(); }

bool isDirectTransition(const Application &) { return false; };

using namespace moodycamel;

bool areAllTransitionsInStore(const Store &store, const Net &net) noexcept {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition =
        std::find_if(store.begin(), store.end(), [&](const auto &e) {
          return e.first == t;
        }) != store.end();
    if (!store_has_transition) {
      spdlog::error("Transition {0} is not in store", t);
    }

    return store_has_transition;
  });
}

/**
 * @brief Petri is the class that holds the implementation of the Petri net. It
 * holds pointers to the reducer queue and that thread pool. Calling `run()` on
 * this class will do the *actual* execution of the Petri net.
 *
 */
struct Petri {
  Model m;  ///< The Petri net model
  std::atomic<std::optional<unsigned int>>
      thread_id_;       ///< The id of the thread from which run is called.
  const Marking m0_;    ///< The initial marking for this instance
  const Net net_;       ///< The net
  const Store &store_;  ///< Reference to the store
  const PriorityTable priorities_;  ///< The priority table for this instance
  const std::vector<size_t>
      final_marking;  ///< The net will stop queueing reducers
                      ///< once the marking has been reached
  const std::shared_ptr<TaskSystem> stp;
  std::shared_ptr<BlockingConcurrentQueue<Reducer>> reducers;
  std::string case_id;  ///< The case id of this particular Petri instance
  std::atomic<bool> early_exit;  ///< once it is true, no more new transitions
                                 ///< will be queued and the run will exit.

  /**
   * @brief Construct a new Petri object. Most importantly, it also creates the
   * reducer queue and exposes the `run` function to actually execute the petri
   * net.
   *
   * @param net
   * @param m0
   * @param stp
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   */
  Petri(const Net &net, const Marking &m0, std::shared_ptr<TaskSystem> stp,
        const Marking &final_marking, const Store &store,
        const PriorityTable &priorities, const std::string &case_id)
      : m(net, store, priorities, m0),
        thread_id_(std::nullopt),
        m0_(m0),
        net_(net),
        store_(store),
        priorities_(priorities),
        final_marking(m.toTokens(final_marking)),
        stp(stp),
        reducers(std::make_shared<BlockingConcurrentQueue<Reducer>>(256)),
        case_id(case_id),
        early_exit(false) {}

  bool reset(const std::string &new_case_id) {
    if (thread_id_.load().has_value() || new_case_id == case_id) {
      return false;
    }

    // recreate model.
    m = Model(net_, store_, priorities_, m0_);
    case_id = new_case_id;
    reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(256);

    early_exit.store(false);
    thread_id_.store(std::nullopt);
    return true;
  }

  /**
   * @brief Get the Event Log object
   *
   * @return Eventlog
   */
  Eventlog getEventLog() const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<Eventlog> el;
      std::future<Eventlog> el_getter = el.get_future();
      reducers->enqueue([&](Model &&model) {
        spdlog::error("[{0}] leftovers", case_id);
        el.set_value(model.event_log);
        return std::ref(model);
      });
      return el_getter.get();
    } else {
      return m.event_log;
    }
  }

  std::pair<std::vector<Transition>, std::vector<Place>> getState()
      const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<std::pair<std::vector<Transition>, std::vector<Place>>>
          state;
      auto getter = state.get_future();
      reducers->enqueue([&](Model &&model) {
        state.set_value(model.getState());
        return std::ref(model);
      });
      return getter.get();
    } else {
      return m.getState();
    }
  }

  std::vector<Transition> getFireableTransitions() const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<std::vector<Transition>> transitions;
      std::future<std::vector<Transition>> transitions_getter =
          transitions.get_future();
      reducers->enqueue([&](Model &&model) {
        transitions.set_value(model.getFireableTransitions());
        return std::ref(model);
      });
      return transitions_getter.get();
    } else {
      return m.getFireableTransitions();
    }
  };

  /**
   * @brief run the petri net. This initializes the net with the initial
   * marking and blocks until it a) reached the final marking, b) deadlocked
   * or c) exited early.
   *
   * @return Result
   */
  Result run() {
    // we are running!
    thread_id_.store(getThreadId());
    early_exit.store(false);
    // reassign it manually to reset.
    m.event_log.clear();
    m.tokens_n = m.initial_tokens;

    Reducer f;
    // start!
    m.fireTransitions(reducers, stp, true, case_id);
    // get a reducer. Immediately, or wait a bit
    while (!early_exit.load() && m.active_transitions_n.size() > 0 &&
           reducers->wait_dequeue_timed(f, -1)) {
      do {
        m = f(std::move(m));
      } while (!early_exit.load() && reducers->try_dequeue(f));
      if (MarkingReached(m.tokens_n, final_marking) || early_exit.load()) {
        break;
      } else {
        m.fireTransitions(reducers, stp, true, case_id);
      }
    }

    // determine what was the reason we terminated.
    State result;
    if (early_exit.load()) {
      {
        // populate that eventlog with child eventlog and possible cancelations.
        for (const auto transition_index : m.active_transitions_n) {
          spdlog::info("[{1}] Cancel {0} ...",
                       m.net.transition[transition_index], case_id);
          auto [el, state] = cancelTransition(m.net.store.at(transition_index));
          spdlog::info("[{1}] {0} cancelled",
                       m.net.transition[transition_index], case_id);
          if (!el.empty()) {
            m.event_log.insert(m.event_log.end(), el.begin(), el.end());
          }
          m.event_log.push_back({case_id, m.net.transition[transition_index],
                                 state, Clock::now()});
        }
        result = State::UserExit;
      }
    } else if (MarkingReached(m.tokens_n, final_marking)) {
      result = State::Completed;
    } else if (m.active_transitions_n.empty()) {
      result = State::Deadlock;
    } else {
      result = State::Error;
    }

    Result res = {m.event_log, result};
    spdlog::info("[{1}] finished with result {0}", printState(result), case_id);
    while (reducers->try_dequeue(f)) {
      spdlog::warn("[{0}] leftovers", case_id);
      m = f(std::move(m));
    }
    thread_id_.store(std::nullopt);
    return res;
  }
};

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
  auto impl = std::make_shared<Petri>(net, m0, stp, final_marking, store,
                                      priority, case_id);
  return {
      impl, [=](const Transition &t) {
        if (impl->thread_id_.load()) {
          impl->reducers->enqueue([=](Model &&m) {
            const auto t_index = toIndex(m.net.transition, t);
            m.active_transitions_n.push_back(t_index);
            impl->reducers->enqueue(createReducerForTransition(
                t_index, m.net.store[t_index], impl->case_id, impl->reducers));
            return std::ref(m);
          });
        }
      }};
}

Application::Application(const std::set<std::string> &files,
                         const Marking &final_marking, const Store &store,
                         const PriorityTable &priorities,
                         const std::string &case_id,
                         std::shared_ptr<TaskSystem> stp) {
  const auto [net, m0] = readPnml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

Application::Application(const std::set<std::string> &files,
                         const Marking &final_marking, const Store &store,
                         const std::string &case_id,
                         std::shared_ptr<TaskSystem> stp) {
  const auto [net, m0, priorities] = readGrml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

Application::Application(const Net &net, const Marking &m0,
                         const Marking &final_marking, const Store &store,
                         const PriorityTable &priorities,
                         const std::string &case_id,
                         std::shared_ptr<TaskSystem> stp) {
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

bool Application::tryFireTransition(const Transition &t) const noexcept {
  if (impl == nullptr) {
    spdlog::warn("There is no net to run a transition.");
    return false;
  }
  Reducer f;
  while (impl->reducers->try_dequeue(f)) {
    impl->m = f(std::move(impl->m));
  }
  return impl->m.fireTransition(t, impl->reducers, impl->stp, "manual");
};

Result Application::execute() const noexcept {
  if (impl == nullptr) {
    spdlog::error("Something went seriously wrong. Please send a bug report.");
    return {{}, State::Error};
  } else if (impl->thread_id_.load()) {
    spdlog::warn(
        "[{0}] is already active, it can not run it again before it is "
        "finished.",
        impl->case_id);
    return {{}, State::Error};
  } else {
    return impl->run();
  }
}

Eventlog Application::getEventLog() const noexcept {
  return impl->getEventLog();
};

std::pair<std::vector<Transition>, std::vector<Place>> Application::getState()
    const noexcept {
  return impl->getState();
}

std::vector<Transition> Application::getFireableTransitions() const noexcept {
  return impl->getFireableTransitions();
};

std::function<void()> Application::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] { register_functor(transition); };
}

Result Application::exitEarly() const noexcept {
  const auto maybe_thread_id = impl->thread_id_.load();
  if (maybe_thread_id && maybe_thread_id.value() != getThreadId() &&
      !impl->early_exit.load()) {
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    impl->reducers->enqueue([=, &m, &cv, &ready](Model &&model) {
      impl->early_exit.store(true);
      {
        std::lock_guard lk(m);
        ready = true;
      }
      cv.notify_one();
      return std::ref(model);
    });
    std::unique_lock lk(m);
    cv.wait(lk, [&] { return ready; });
  } else {
    impl->reducers->enqueue([=](Model &&model) {
      impl->early_exit.store(true);
      return std::ref(model);
    });
  }

  return {getEventLog(), State::UserExit};
}

bool Application::reuseApplication(const std::string &new_case_id) {
  return impl->reset(new_case_id);
}

}  // namespace symmetri
