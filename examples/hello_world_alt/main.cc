#include <chrono>
#include <future>
#include <thread>

#include "Symmetri/symmetri.h"
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

int main(int argc, char *argv[]) {
  auto pnml_path_start = std::string(argv[1]);
  auto store = symmetri::TransitionActionMap{{"t0", &helloWorld},
                                             {"t1", &helloWorld},
                                             {"t2", &helloWorld},
                                             {"t3", &helloWorld},
                                             {"t4", &helloWorld}};

  symmetri::Application net({pnml_path_start}, store, "ExtLoop");

  net();  // infinite loop
}
