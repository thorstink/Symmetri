#include "Symmetri/symmetri.h"
#include <iostream>

symmetri::OptionalError helloWorld() {
  std::cout << "hello world!" << std::endl;
  return std::nullopt;
}

int main(int argc, char *argv[]) {
  auto pnml_path = std::string(argv[1]);
  auto store = symmetri::TransitionActionMap{{"t0", &helloWorld}};
  auto go = symmetri::start(pnml_path, store);
  go(); // infinite loop
}
