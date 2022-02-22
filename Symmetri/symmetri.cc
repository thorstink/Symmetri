#include "Symmetri/symmetri.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <functional>
#include <memory>
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
  const auto &[Dm, Dp, M0, arc_list, transitions, places] =
      constructTransitionMutationMatrices(files);

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

    auto stp = executeTransition(store, places, reducers, actions, M0.size(), 3,
                                 case_id);
    auto m = Model(clock_t::now(), M0, Dm, Dp, actions);

    // auto start
    reducers.enqueue(noop);

    Reducer f = noop;
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
      if (server.has_value() &&
          server.value()->marking_transition->hasConnections()) {
        auto data = (*m.data);

        server.value()->queueTask(
            [=, net = genNet(arc_list,
                             getPlaceMarking(places.id_to_label_, m.data->M),
                             m.data->active_transitions)]() {
              server.value()->marking_transition->send(net);
            });
        server.value()->queueTask([=, log = logToCsv(data.log)]() {
          server.value()->time_data->send(log);
        });
      }
      m.data->log.clear();

      if (m.data->active_transitions.size() == 0) {
        break;
      }
    };
    stp.stop();
  };
}

void Application::operator()() const { return run(); };

}  // namespace symmetri
