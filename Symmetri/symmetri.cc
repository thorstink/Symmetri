#include "Symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <signal.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <tuple>

#include "actions.h"
#include "immer/algorithm.hpp"
#include "model.h"
#include "pnml_parser.h"
namespace symmetri {
// Define the function to be called when ctrl-c (SIGINT) is sent to process
bool EARLY_EXIT = false;
inline void signal_handler(int signal) { EARLY_EXIT = true; }

using namespace moodycamel;

size_t calculateTrace(Eventlog event_log) {
  // calculate a trace-id, in a simple way.
  return std::hash<std::string>{}(immer::accumulate(
      event_log.begin(), event_log.end(), std::string(""),
      [](const auto &acc, const Event &n) {
        return n.state == TransitionState::Completed ? acc + n.transition : acc;
      }));
}

std::string printState(symmetri::TransitionState s) {
  std::string ret;
  switch (s) {
    case TransitionState::Started:
      ret = "Started";
      break;
    case TransitionState::Completed:
      ret = "Completed";
      break;
    case TransitionState::Deadlock:
      ret = "Deadlock";
      break;
    case TransitionState::UserExit:
      ret = "UserExit";
      break;
    case TransitionState::Error:
      ret = "Error";
      break;
  }

  return ret;
}

bool check(const Store &store, const symmetri::StateNet &net) {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition = store.contains(t);
    if (!store_has_transition) {
      spdlog::error("Transition {0} is not in store", t);
    }

    return store_has_transition;
  });
}

struct Impl {
  Model m;
  BlockingConcurrentQueue<Reducer> reducers;
  BlockingConcurrentQueue<PolyAction> polymorphic_actions;
  StoppablePool stp;
  const std::string case_id;
  std::optional<symmetri::NetMarking> final_marking;
  ~Impl() {
    EARLY_EXIT = true;
    stp.stop();
  }
  Impl(const symmetri::StateNet &net, const symmetri::NetMarking &m0,
       const std::optional<symmetri::NetMarking> &final_marking,
       const Store &store, unsigned int thread_count,
       const std::string &case_id)
      : m(net, store, m0),
        reducers(256),
        polymorphic_actions(256),
        stp(thread_count, polymorphic_actions),
        case_id(case_id),
        final_marking(final_marking) {}
  const Model &getModel() const { return m; }
  TransitionResult run() {
    const auto m0 = m.M;
    // enqueue a noop to start, not
    // doing this would require
    // external input to 'start' (even if the petri net is alive)
    reducers.enqueue([](Model &&m) -> Model & { return m; });

    auto stop_condition =
                     final_marking.has_value()?[&] {
                           return EARLY_EXIT || (m.pending_transitions.empty() && m.M != m0) ||
                                  MarkingReached(m.M, final_marking.value());
                         }
                         : std::function{[&] { return EARLY_EXIT || (m.pending_transitions.empty() && m.M != m0); }};
    Reducer f;
    do {
      // get a reducer.
      while ((m.pending_transitions.empty() && m.M != m0)
                 ? reducers.try_dequeue(f)
                 : reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
        if (EARLY_EXIT) {
          break;
        }
        try {
          m = runAll(f(std::move(m)), reducers, polymorphic_actions, case_id);
        } catch (const std::exception &e) {
          spdlog::get(case_id)->error(e.what());
        }
      }
    } while (!stop_condition());

    // determine what was the reason we terminated.
    TransitionState result;
    if (final_marking.has_value() ? MarkingReached(m.M, final_marking.value())
                                  : false) {
      result = TransitionState::Completed;
    } else if (EARLY_EXIT) {
      result = TransitionState::UserExit;
    } else if (m.pending_transitions.empty()) {
      result = TransitionState::Deadlock;
    } else {
      result = TransitionState::Error;
    }

    // stop the thread pool, blocks.
    stp.stop();

    // publish a log
    spdlog::get(case_id)->info(
        printState(result) + " of {0}-net. Trace-hash is {1}", case_id,
        calculateTrace(m.event_log));
    return {m.event_log, result};
  }
};

Application::Application(
    const std::set<std::string> &files,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store, unsigned int thread_count, const std::string &case_id) {
  const auto &[net, m0] = readPetriNets(files);
  createApplication(net, m0, final_marking, store, thread_count, case_id);
}

Application::Application(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store, unsigned int thread_count, const std::string &case_id) {
  createApplication(net, m0, final_marking, store, thread_count, case_id);
}

void Application::createApplication(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store, unsigned int thread_count, const std::string &case_id) {
  signal(SIGINT, signal_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());
  if (check(store, net)) {
    // init
    impl = std::make_shared<Impl>(net, m0, final_marking, store, thread_count,
                                  case_id);
    // register a function that "forces" transitions into the queue.
    p = [this](const std::string &t) {
      impl->polymorphic_actions.enqueue([t, this, &task = impl->m.store.at(t)] {
        impl->reducers.enqueue(runTransition(t, task, impl->case_id));
      });
    };
  }
}

TransitionResult Application::operator()() const {
  if (impl == nullptr) {
    spdlog::error("Error not all transitions are in the store");
    return {{}, TransitionState::Error};
  } else {
    return impl->run();
  }
}

std::tuple<clock_s::time_point, symmetri::Eventlog, symmetri::StateNet,
           symmetri::NetMarking, std::set<std::string>>
Application::get() const {
  const auto &m = impl->getModel();
  return std::make_tuple(m.timestamp, m.event_log, m.net, m.M,
                         m.pending_transitions);
}

}  // namespace symmetri
