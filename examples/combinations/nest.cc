#include "foo.hpp"
#include "symmetri/parsers.h"
#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"

using namespace std::chrono_literals;

using namespace symmetri;

int main(int, char *argv[]) {
  const auto tasks = std::string(argv[1]);
  const auto single_step_processor = std::string(argv[2]);
  const auto dual_step_processor = std::string(argv[3]);

  auto pool = std::make_shared<TaskSystem>(2);
  // the child net
  PetriNet child_net(
      std::get<Net>(readPnml({dual_step_processor})),
      {{"TaskBucket", Color::toString(Color::Success)},
       {"ResourceDualProcessor", Color::toString(Color::Success)}},
      {{"SuccessfulTasks", Color::toString(Color::Success)},
       {"ResourceDualProcessor", Color::toString(Color::Success)}},
      {{"StepOne", Foo{0.75, 1ms}}, {"StepTwo", Foo{0.75, 2ms}}}, {},
      "child_net", pool);

  // for the parent net:
  const auto read = readPnml({tasks, single_step_processor});
  const Net net = std::get<Net>(read);
  const Marking initial_marking = std::get<Marking>(read);
  const auto goal_marking = getGoal(initial_marking);

  PetriNet parent_net(net, initial_marking, goal_marking,
                      {{"SingleStepProcessor", child_net}}, {}, "parent_net",
                      pool);

  // run!
  auto now = Clock::now();
  auto result = fire(parent_net);
  auto dt =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - now);

  printLog(getLog(parent_net));

  spdlog::info("Token of this net: {0}. It took {1} ms, token count: {2}",
               Color::toString(result), dt.count(),
               parent_net.getMarking().size());

  return 0;
}
