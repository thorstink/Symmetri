#include "Symmetri/symmetri.h"

#include <fstream>
#include <functional>
#include <tuple>

#include "Symmetri/types.h"
#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "ws_interface.hpp"

namespace symmetri {
using namespace moodycamel;
using namespace seasocks;

constexpr auto noop = [](const Model &m) { return m; };

BlockingConcurrentQueue<std::function<void()>> web(256);
std::thread web_t_;
std::shared_ptr<Wsio> marking_transition;
std::shared_ptr<Output> time_data;

void webIFace(const nlohmann::json &json_net) {
  seasocks::Server server(
      std::make_shared<seasocks::PrintfLogger>(seasocks::Logger::Level::Error));
  time_data = std::make_shared<Output>();
  marking_transition = std::make_shared<Wsio>(json_net);
  server.addWebSocketHandler("/transition_data", time_data);
  server.addWebSocketHandler("/marking_transition_data", marking_transition);
  server.startListening(2222);
  server.setStaticPath("web");
  std::cout << "interface online at http://localhost:2222/" << std::endl;

  std::function<void()> task;
  while (true) {
    while (web.try_dequeue(task)) {
      task();
    }
    server.poll(10);
  }
}

std::function<void()> start(const std::string &pnml_path,
                            const TransitionActionMap &store) {
  return [=]() {
    const auto &[Dm, Dp, M0, json_net, transitions, places] =
        constructTransitionMutationMatrices(pnml_path);

    web_t_ = std::thread(&webIFace, json_net);

    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(1024);

    auto tp = executeTransition(store, places, reducers, actions, M0.size(), 3);
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
      while (reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
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

      nlohmann::json j;
      j["active_transitions"] = nlohmann::json(m.data->active_transitions);
      j["marking"] = webMarking(m.data->M, places.id_to_label_);

      web.try_enqueue([j]() { marking_transition->send(j.dump()); });
      web.try_enqueue([l = log_data.str()]() { time_data->send(l); });
    };
    web_t_.join();
  };
}
}  // namespace symmetri
