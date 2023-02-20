#include "symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <signal.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <sstream>
#include <tuple>

#include "model.h"
#include "symmetri/pnml_parser.h"

namespace symmetri {

std::condition_variable cv;
std::mutex cv_m;  // This mutex is used for two purposes:
                  // 1) to synchronize accesses to PAUSE & EARLY_EXIT
                  // 2) for the condition variable cv

std::atomic<bool> PAUSE(false);
std::atomic<bool> EARLY_EXIT(false);

Result fireTransition(const Application &app) { return app.execute(); };

// The default exit handler just sets early exit to true.
std::function<void()> EARLY_EXIT_HANDLER = []() {
  spdlog::info("User requests exit");
  std::lock_guard<std::mutex> lk(cv_m);
  EARLY_EXIT.store(true);
  cv.notify_all();
};

void blockIfPaused(const std::string &case_id) {
  std::unique_lock<std::mutex> lk(cv_m);
  if (PAUSE.load(std::memory_order_relaxed)) {
    spdlog::get(case_id)->info("Execution is paused");
    cv.wait(lk, [] { return !PAUSE.load() || EARLY_EXIT.load(); });
    spdlog::get(case_id)->info("Execution is resumed");
  }
}

// Define the function to be called when ctrl-c (SIGINT) is sent to process
inline void exit_handler(int) noexcept { EARLY_EXIT_HANDLER(); }

using namespace moodycamel;

bool check(const Store &store, const Net &net) noexcept {
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
  Model m;            ///< The Petri net model
  const Marking m0_;  ///< The initial marking for this instance
  std::shared_ptr<BlockingConcurrentQueue<Reducer>> reducers;
  std::shared_ptr<const StoppablePool> stp;
  const std::string case_id;  ///< The case id of this particular Petri instance
  std::atomic<bool>
      active;  ///< The net is active as long as it is still dequeuing reducers
  Marking final_marking;  ///< The net will stop queueing reducers
                          ///< once the marking has been reached

  /**
   * @brief Construct a new Petri object. Most importantly, it also creates the
   * reducer queue and exposes the `run` function to actually execute the petri
   * net. During construction some `EARLY_EXIT_HANDLER` is reassigned to a
   * function that sets EARLY_EXIT to true and queues an empty reducer. This is
   * done to force the check for an early exit.
   *
   * @param net
   * @param m0
   * @param stp
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   */
  Petri(const Net &net, const Marking &m0,
        std::shared_ptr<const StoppablePool> stp, const Marking &final_marking,
        const Store &store,
        const std::vector<std::pair<Transition, int8_t>> &priority,
        const std::string &case_id)
      : m(net, store, priority, m0),
        m0_(m0),
        reducers(std::make_shared<BlockingConcurrentQueue<Reducer>>(256)),
        stp(stp),
        case_id(case_id),
        active(false),
        final_marking(final_marking) {
    EARLY_EXIT_HANDLER = [case_id, this]() {
      spdlog::get(case_id)->info("User requests exit");
      std::lock_guard<std::mutex> lk(cv_m);
      if (!EARLY_EXIT.load()) {
        EARLY_EXIT.store(true);
      }
      reducers->enqueue([](Model &&model) -> Model & { return model; });
      cv.notify_all();
    };
  }

  /**
   * @brief Get the Model object
   *
   * @return const Model&
   */
  const Model &getModel() const { return m; }

  /**
   * @brief Get the Event Log object
   *
   * @return Eventlog
   */
  Eventlog getEventLog() const { return m.event_log; }

  /**
   * @brief run the petri net. This initializes the net with the initial marking
   * and blocks until it a) reached the final marking, b) deadlocked or c)
   * exited early.
   *
   * @return Result
   */
  Result run() {
    // we are running!
    active.store(true);
    // todo.. not have to assign it manually to reset.
    m.event_log = {};
    m.tokens_n = m.initial_tokens;
    std::vector<size_t> final_tokens;
    for (const auto &[p, c] : final_marking) {
      for (int i = 0; i < c; i++) {
        final_tokens.push_back(toIndex(m.net.place, p));
      }
    }

    Reducer f;
    // start!
    m.fireTransitions(reducers, *stp, true, case_id);
    // get a reducer. Immediately, or wait a bit
    while (m.active_transitions_n.size() > 0 &&
           !EARLY_EXIT.load(std::memory_order_relaxed) &&
           reducers->wait_dequeue_timed(f, -1) &&
           !EARLY_EXIT.load(std::memory_order_relaxed)) {
      do {
        m = f(std::move(m));
      } while (reducers->try_dequeue(f));
      blockIfPaused(case_id);
      if (MarkingReached(m.tokens_n, final_tokens)) {
        break;
      }
      m.fireTransitions(reducers, *stp, true, case_id);
    }
    active.store(false);

    // determine what was the reason we terminated.
    State result;
    if (EARLY_EXIT.load()) {
      result = State::UserExit;
    } else if (MarkingReached(m.tokens_n, final_tokens)) {
      result = State::Completed;
    } else if (m.active_transitions_n.empty()) {
      result = State::Deadlock;
    } else {
      result = State::Error;
    }
    spdlog::get(case_id)->info("Petri net finished with result {0}",
                               printState(result));

    return {m.event_log, result};
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
       const Store &store,
       const std::vector<std::pair<Transition, int8_t>> &priority,
       const std::string &case_id, std::shared_ptr<const StoppablePool> stp) {
  signal(SIGINT, exit_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);
  console->set_pattern(s.str());
  auto impl = std::make_shared<Petri>(net, m0, stp, final_marking, store,
                                      priority, case_id);
  return {impl, [=](const Transition &t) {
            if (impl->active.load()) {
              impl->reducers->enqueue([=](Model &&m) -> Model & {
                const auto t_index = toIndex(m.net.transition, t);
                m.active_transitions_n.push_back(t_index);
                impl->reducers->enqueue(createReducerForTransition(
                    t_index, m.net.store[t_index], impl->case_id));
                return m;
              });
            }
          }};
}

Application::Application(
    const std::set<std::string> &files, const Marking &final_marking,
    const Store &store,
    const std::vector<std::pair<Transition, int8_t>> &priority,
    const std::string &case_id, std::shared_ptr<const StoppablePool> stp) {
  const auto &[net, m0] = readPetriNets(files);
  if (check(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priority, case_id, stp);
  }
}

Application::Application(
    const Net &net, const Marking &m0, const Marking &final_marking,
    const Store &store,
    const std::vector<std::pair<Transition, int8_t>> &priority,
    const std::string &case_id, std::shared_ptr<const StoppablePool> stp) {
  if (check(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priority, case_id, stp);
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
  return impl->m.fireTransition(t, impl->reducers, *impl->stp, "manual");
};

Result Application::execute() const noexcept {
  if (impl == nullptr) {
    spdlog::error("Something went seriously wrong. Please send a bug report.");
    return {{}, State::Error};
  } else if (impl->active.load()) {
    spdlog::get(impl->case_id)
        ->warn(
            "The net is already active, it can not run it again before it is "
            "finished.");
    return {{}, State::Error};
  } else {
    auto res = impl->run();
    return res;
  }
}

Eventlog Application::getEventLog() const noexcept {
  if (impl->active.load()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    impl->reducers->enqueue([&](Model &&model) -> Model & {
      el.set_value(model.event_log);
      return model;
    });
    return el_getter.get();
  } else {
    return impl->getEventLog();
  }
};

std::pair<std::vector<Transition>, std::vector<Place>> Application::getState()
    const noexcept {
  if (impl->active.load()) {
    std::promise<std::pair<std::vector<Transition>, std::vector<Place>>> state;
    auto getter = state.get_future();
    impl->reducers->enqueue([&](Model &&model) -> Model & {
      state.set_value(model.getState());
      return model;
    });
    return getter.get();
  } else {
    return impl->getModel().getState();
  }
}

std::vector<Transition> Application::getFireableTransitions() const noexcept {
  if (impl->active.load()) {
    std::promise<std::vector<Transition>> transitions;
    std::future<std::vector<Transition>> transitions_getter =
        transitions.get_future();
    impl->reducers->enqueue([&](Model &&model) -> Model & {
      transitions.set_value(model.getFireableTransitions());
      return model;
    });
    return transitions_getter.get();
  } else {
    return impl->getModel().getFireableTransitions();
  }
};

std::function<void()> Application::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this]() { register_functor(transition); };
}

void Application::exitEarly() const noexcept { EARLY_EXIT_HANDLER(); }

void Application::togglePause() const noexcept {
  std::lock_guard<std::mutex> lk(cv_m);
  PAUSE.store(!PAUSE.load());
  cv.notify_all();
}

}  // namespace symmetri
