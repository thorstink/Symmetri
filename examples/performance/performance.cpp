#include <symmetri/symmetri.h>

#include <iostream>
#include <symmetri/utilities.hpp>
struct Simple {};
int i = 0;
bool isSynchronous(const Simple &) { return true; }
symmetri::Token fire(const Simple &) {
  i++;
  return symmetri::Color::Success;
}

int main(int argc, char *argv[]) {
  using namespace symmetri;
  auto pool = std::make_shared<TaskSystem>(1);
  PetriNet petri = [=] {
    if (argc > 1) {
      const auto pnml = std::string(argv[1]);
      return PetriNet({pnml}, "benchmark", pool);
    } else {
      Net net = {{"t0",
                  {{{"Pa", Color::Success}, {"Pb", Color::Success}},
                   {{"Pc", Color::Success}}}},
                 {"t1",
                  {{{"Pc", Color::Success}, {"Pc", Color::Success}},
                   {{"Pb", Color::Success}, {"Pb", Color::Success}}}}};
      Marking initial_marking = {
          {"Pa", Color::Success}, {"Pa", Color::Success},
          {"Pa", Color::Success}, {"Pa", Color::Success},
          {"Pb", Color::Success}, {"Pb", Color::Success}};
      Marking goal_marking = {{"Pb", Color::Success},
                              {"Pb", Color::Success},
                              {"Pc", Color::Success},
                              {"Pc", Color::Success}};

      PetriNet petri(net, "benchmark", pool, initial_marking, goal_marking);
      petri.registerCallback("t0", Simple{});
      petri.registerCallback("t1", Simple{});
      return petri;
    }
  }();
  const auto begin = Clock::now();
  std::cout << "start" << std::endl;
  const auto result = fire(petri);
  const auto end = Clock::now();
  std::cout << i << " transitions in "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << ", execution trace: " << calculateTrace(getLog(petri))
            << std::endl;
  return result == Color::Success ? 0 : -1;
}
