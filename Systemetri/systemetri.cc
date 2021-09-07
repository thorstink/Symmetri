#include "systemetri.h"
#include "actions.h"
#include "pnml_parser.h"
#include "types.h"
#include "ws_interface.hpp"
#include <csignal>
#include <rxcpp/rx.hpp>

namespace systemetri {
using namespace rxcpp::rxo;
using namespace rxcpp;
using namespace types;
using namespace model;
using namespace actions;

std::function<void()> start(const std::string &pnml_path,
                            const model::TransitionActionMap &store) {

  return [=]() {
    rxcpp::composite_subscription lifetime;
    schedulers::run_loop rl;
    const auto &[Dm, Dp, M0] = constructTransitionMutationMatrices(pnml_path);

    seasocks::Server server(std::make_shared<seasocks::PrintfLogger>(
        seasocks::Logger::Level::Error));

    const auto mainthread = rxcpp::observe_on_new_thread();
    const auto viewthread = observe_on_run_loop(rl);

    const rxcpp::subjects::subject<Transition> transitions(lifetime);

    const std::array<rxcpp::observable<Reducer>, 2> reducers = {
        serverSource(server),
        transitions.get_observable() |
            flat_map(executeTransition(rxcpp::observe_on_event_loop(), store))};

    const auto actions = iterate(reducers) | merge(mainthread);
    const auto models =
        actions |
        scan(Model(model::clock_t::now(), M0, Dm, Dp),
             [&mainthread, dispatcher = transitions.get_subscriber()](
                 Model m, const Reducer &f) {
               try {
                 // run the reducer
                 auto [r, transitions] = run(f(m));
                 // dispatch the transitions
                 for (auto transition : transitions)
                   dispatcher.on_next(transition);

                 r.data->timestamp = model::clock_t::now();
                 return r;
               } catch (const std::exception &e) {
                 std::cerr << e.what() << '\n';
                 return m;
               }
             }) |
        // only view model updates every 1000ms
        sample_with_time(std::chrono::milliseconds(1000), mainthread) |
        publish(lifetime) | ref_count() | as_dynamic();

    auto output = std::make_shared<Output>();

    const auto views = models.map([](const Model model) {
      auto data = (*model.data);
      model.data->log.clear();
      return data;
    }) | observe_on(viewthread) |
                       tap([output](const model::Model::shared &data) {
                         std::stringstream log_data;
                         for (const auto &[a, b, c, d] : data.log) {
                           log_data << a << ',' << b << ',' << c << ',' << d
                                    << '\n';
                         }
                         output->send(log_data.str());
                       });

    views.subscribe(
        lifetime, [](const model::Model::shared &) {},
        [](rxcpp::util::error_ptr ep) {
          printf("OnError: %s\n", rxcpp::util::what(ep).c_str());
        },
        [] {});

    server.addWebSocketHandler("/transition_data", output);
    server.startListening(2222);
    server.setStaticPath("web");
    std::cout << "interface online at http://localhost:2222/" << std::endl;

    while (lifetime.is_subscribed() || !rl.empty()) {
      while (!rl.empty() && rl.peek().when < rl.now()) {
        rl.dispatch();
      }
      server.poll(50);
    }
  };
}
} // namespace systemetri
