#include "Symmetri/symmetri.h"
#include "Symmetri/types.h"
#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "ws_interface.hpp"
#include <fstream>
#include <functional>
#include <tuple>

namespace symmetri {
using namespace moodycamel;
using namespace seasocks;

constexpr auto noop = [](const Model &m) { return m; };

std::function<void()> start(const std::string &pnml_path,
                            const TransitionActionMap &store) {

  return [=]() {
    const auto &[Dm, Dp, M0, json_net, index_place_map] =
        constructTransitionMutationMatrices(pnml_path);

    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(1024);

    seasocks::Server server(std::make_shared<seasocks::PrintfLogger>(
        seasocks::Logger::Level::Error));
    auto time_data = std::make_shared<Output>();
    auto marking_transition = std::make_shared<Wsio>(json_net);
    server.addWebSocketHandler("/transition_data", time_data);
    server.addWebSocketHandler("/marking_transition_data", marking_transition);
    server.startListening(2222);
    server.setStaticPath("web");
    std::cout << "interface online at http://localhost:2222/" << std::endl;

    auto tp = executeTransition(store, reducers, actions, 3);
    auto m = Model(clock_t::now(), M0, Dm, Dp, actions);

    // auto start
    reducers.enqueue(noop);

    std::ofstream output_file;
    std::ofstream json_output_file;
    json_output_file.open("net.json");
    json_output_file << json_net.dump(2);
    json_output_file.close();

    Reducer f = noop;
    while (true) {
      // get a reducer.
      while (reducers.try_dequeue(f)) {
        m.data->timestamp = clock_t::now();
        try {
          m = run_all(f(m));
        } catch (const std::exception &e) {
          std::cerr << e.what() << '\n';
        }
      }

      auto data = (*m.data);
      m.data->log.clear();
      std::stringstream log_data;
      for (const auto &[a, b, c, d] : data.log) {
        log_data << a << ',' << b << ',' << c << ',' << d << '\n';
      }

      // output_file.open("example2.csv",std::ios_base::app);
      // output_file << log_data.str();
      // output_file.close();
      nlohmann::json j;
      j["active_transitions"] = nlohmann::json(m.data->active_transitions);
      j["marking"] = webMarking(m.data->M, index_place_map);
      marking_transition->send(j.dump());
      time_data->send(log_data.str());
      server.poll(10);
    };
  };
}
} // namespace symmetri
