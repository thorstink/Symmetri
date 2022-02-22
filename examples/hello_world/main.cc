#include <chrono>
#include <future>
#include <iostream>
#include <thread>

#include "Symmetri/symmetri.h"
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

int main(int argc, char *argv[]) {
  auto pnml_path = std::string(argv[1]);
  auto store = symmetri::TransitionActionMap{
      {"t0", &helloWorld}, {"t1", &helloWorld}, {"t2", &helloWorld},
      {"t3", &helloWorld}, {"t4", &helloWorld}, {"t99", &helloWorld}};

  symmetri::Application net({pnml_path}, store);

  auto t = std::async(std::launch::async, [f = net.push<float>("t99")] {
    float a;
    std::cin >> a;
    f(a);
  });

  net();  // infinite loop
}
