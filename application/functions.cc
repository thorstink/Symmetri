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
    {"t0", &action0}, {"t1", &action1}, {"t2", &action2}, {"t3", &action3},
    {"t4", &action4}, {"t5", &action5}, {"t6", &action6}};

// const static TransitionActionMap local_store = {
//     {"t18", &action0}, {"t19", &action1}, {"t20", &action2}, {"t21", &action3},
//     {"t22", &action4}, {"t23", &action5}, {"t24", &action6}, {"t25", &action3},
//     {"t26", &action4}, {"t27", &action5}, {"t28", &action6}, {"t29", &action6}};

OptionalReducer execute(const transitions::Transition &transition) {
  auto fun_ptr = local_store.find(transition);
  return (fun_ptr != local_store.end())
             ? fun_ptr->second()
             : model::Reducer([=](model::Model model) {
                 throw std::runtime_error("the following Transition is not in the store: '" + transition);
                 return model;
               });
}
