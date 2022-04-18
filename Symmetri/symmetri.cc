#include "Symmetri/symmetri.h"

#include <signal.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>

#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "types.h"
#include "ws_interface.hpp"

// Define the function to be called when ctrl-c (SIGINT) is sent to process

namespace {
bool EXIT = false;
inline void signal_handler(int signal) { EXIT = true; }
}  // namespace

namespace symmetri {
using namespace moodycamel;

constexpr auto noop = [](Model &&m) { return std::forward<Model>(m); };

bool check(const TransitionActionMap &store, const symmetri::StateNet &net) {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition = store.contains(t);
    if (!store_has_transition) {
      spdlog::error("Transition {0} is not in store", t);
    }

    return store_has_transition;
  });
}

Application::Application(const std::set<std::string> &files,
                         const TransitionActionMap &store,
                         const std::string &case_id, bool interface) {
  const auto &[net, m0] = constructTransitionMutationMatrices(files);

  signal(SIGINT, signal_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());
  if (!check(store, net)) {
    spdlog::get(case_id)->error("Error not all transitions are in the store");
  }
  run = [=, this]() -> std::vector<Event> {
    auto server =
        interface ? std::optional(WsServer::Instance()) : std::nullopt;
    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(256);

    // register a function that puts transitions into the queue.
    p = [&a = actions](const std::string &t) { a.enqueue(t); };

    // returns a simple stoppable threadpool.
    auto stp = executeTransition(store, reducers, actions, 3, case_id);

    // the initial state
    auto m = Model(clock_t::now(), net, m0);

    // auto start
    reducers.enqueue(noop);

    Reducer f;
    Transitions T;

    while (!EXIT) {
      // get a reducer.
      while ((m.data->pending_transitions.empty() && !m.data->trace.empty())
                 ? reducers.try_dequeue(f)
                 : reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
        m.data->timestamp = clock_t::now();
        try {
          std::tie(m, T) = run_all(f(std::move(m)));
          actions.enqueue_bulk(T.begin(), T.size());

        } catch (const std::exception &e) {
          spdlog::get(case_id)->error(e.what());
        }
      }

      // server stuffies
      if (server.has_value()) {
        server.value()->sendNet(m.data->timestamp, m.data->net, m.data->M,
                                m.data->pending_transitions,
                                m.data->transition_end_times);
        server.value()->sendLog(m.data->log);
      };
      m.data->log.clear();

      // end critiria. If there are no active transitions anymore.
      if (m.data->pending_transitions.empty() && !m.data->trace.empty()) {
        break;
      }
    };

    // calculate a trace-id
    auto trace_hash = std::hash<std::string>{}(std::accumulate(
        m.data->trace.begin(), m.data->trace.end(), std::string("trace:")));

    // publish a log
    spdlog::get(case_id)->info(
        std::string(EXIT ? "Forced shutdown" : "Deadlock") +
            " of {0}-net. End trace is {1}",
        case_id, trace_hash);

    // stop the thread pool
    stp.stop();
    // if we have a viz server, kill it.
    if (server.has_value()) {
      server.value()->stop();
    }

    return m.data->event_log;
  };
}

std::vector<Event> Application::operator()() const { return run(); };

}  // namespace symmetri
