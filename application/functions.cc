#include "functions.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_map>

void sleep(std::chrono::milliseconds ms) {
  auto start = std::chrono::high_resolution_clock::now();
  while ((std::chrono::high_resolution_clock::now() - start) < ms) {
    std::this_thread::yield();
  }
  return;
}

OptionalReducer action0() {
  std::cout << "Executing Transition 0 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(190));
  return std::nullopt;
}
OptionalReducer action1() {
  std::cout << "Executing Transition 1 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(700));

  return model::Reducer([](model::Model model) {
    // if you'd really want, here you could put stuff in the model.
    return model;
  });
}
OptionalReducer action2() {
  std::cout << "Executing Transition 2 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(200));
  return std::nullopt;
}
OptionalReducer action3() {
  std::cout << "Executing Transition 3 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(600));
  return std::nullopt;
}
OptionalReducer action4() {
  std::cout << "Executing Transition 4 on thread " << std::this_thread::get_id()
            << '\n';
  auto dur = 200 + std::rand() / ((RAND_MAX + 1500u) / 1500);
  sleep(std::chrono::milliseconds(dur));
  return std::nullopt;
}

OptionalReducer action5() {
  std::cout << "Executing Transition 5 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(450));
  return std::nullopt;
}

OptionalReducer action6() {
  std::cout << "Executing Transition 6 on thread " << std::this_thread::get_id()
            << '\n';
  sleep(std::chrono::milliseconds(250));
  return std::nullopt;
}

using TransitionActionMap =
    std::unordered_map<transitions::Transition,
                       std::function<OptionalReducer()>>;

const static TransitionActionMap local_store = {
    {0, &action0}, {1, &action1}, {2, &action2}, {3, &action3},
    {4, &action4}, {5, &action5}, {6, &action6}};

OptionalReducer execute(const transitions::Transition &transition) {
  return local_store.at(transition)();
}
