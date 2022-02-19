#include <chrono>
#include <iostream>
#include <thread>

#include "Symmetri/symmetri.h"
symmetri::OptionalError helloWorld() {
  std::cout << "hello world!" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return std::nullopt;
}

int main(int argc, char *argv[]) {
  auto pnml_path = std::string(argv[1]);
  auto store = symmetri::TransitionActionMap{{"t0", &helloWorld},
                                             {"t1", &helloWorld},
                                             {"t2", &helloWorld},
                                             {"t3", &helloWorld},
                                             {"t4", &helloWorld}};
  auto go = symmetri::start({pnml_path}, store);
  go();  // infinite loop
}
