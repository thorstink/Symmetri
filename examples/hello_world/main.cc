#include <chrono>
#include <future>
#include <iostream>
#include <thread>

#include "Symmetri/symmetri.h"
symmetri::OptionalError helloWorld() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return std::nullopt;
}

int main(int argc, char *argv[]) {
  symmetri::input external_inputs;

  auto t = std::async(std::launch::async, [&external_inputs] {
    float a;
    std::cin >> a;
    std::cout << a << std::endl;
    // generate a callback.
    auto f = external_inputs.push<float>("t99");
    // call it.
    f(a);
  });

  auto pnml_path = std::string(argv[1]);
  auto store = symmetri::TransitionActionMap{
      {"t0", &helloWorld}, {"t1", &helloWorld}, {"t2", &helloWorld},
      {"t3", &helloWorld}, {"t4", &helloWorld}, {"t99", &helloWorld}};

  auto go = symmetri::start({pnml_path}, store, external_inputs);
  go();  // infinite loop
}
