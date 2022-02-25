#include "Symmetri/symmetri.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>

#include "actions.h"
#include "mermaid.hpp"
#include "model.h"
#include "pnml_parser.h"
#include "types.h"
#include "ws_interface.hpp"

namespace symmetri {
using namespace moodycamel;

constexpr auto noop = [](const Model &m) { return m; };

Application::Application(const std::set<std::string> &files,
                         const TransitionActionMap &store,
                         const std::string &case_id, bool interface) {
  const auto &[net, m0] = constructTransitionMutationMatrices(files);

  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());

  run = [=, this]() {
    auto server =
        interface ? std::optional(WsServer::Instance()) : std::nullopt;
    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(256);

    // register a function that puts transitions into the queue.
    p = [&a = actions](const std::string &t) { a.enqueue(t); };

    auto stp = executeTransition(store, reducers, actions, 3, case_id);
    auto m = Model(clock_t::now(), net, m0, actions);

    // auto start
    reducers.enqueue(noop);

    Reducer f;
    while (true) {
      // get a reducer.
      while (reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
        m.data->timestamp = clock_t::now();
        try {
          m = run_all(f(m));
        } catch (const std::exception &e) {
          spdlog::get(case_id)->error(e.what());
        }
      }

      // server stuffies
      if (server.has_value()) {
        server.value()->sendNet(
            genNet(m.data->net, m.data->M, m.data->active_transitions));
        server.value()->sendLog(logToCsv(m.data->log));
      };
      m.data->log.clear();

      if (m.data->active_transitions.size() == 0 && !m.data->trace.empty()) {
        auto sum = std::hash<std::string>{}(std::accumulate(
            m.data->trace.begin(), m.data->trace.end(), std::string("trace:")));
        spdlog::get(case_id)->info("Deadlock of {0}-net. End trace is {1}",
                                   case_id, sum);
        break;
      }
    };
    if (server.has_value()) {
      server.value()->stop();
    }
    stp.stop();
  };
}

void Application::operator()() const { return run(); };

}  // namespace symmetri
