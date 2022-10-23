#include <catch2/catch_test_macros.hpp>

#include "Symmetri/symmetri.h"
#include "actions.h"

using namespace symmetri;
using namespace moodycamel;
TEST_CASE("Run the executor") {
  // Create a simple stoppable threadpool with 1 thread.
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(256);
  StoppablePool stp(1, polymorphic_actions);

  // launch a task
  std::atomic<bool> ran = false;
  polymorphic_actions.enqueue([&]() { ran = true; });
  // wait a little before shutting the thread pool down.
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  REQUIRE(ran);
  // stop the pool.
  stp.stop();
}

TEST_CASE("Run the executor parallel tasks") {
  // Create a simple stoppable threadpool with 2 threads.
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(256);
  StoppablePool stp(2, polymorphic_actions);

  // launch a task
  std::atomic<bool> ran1 = false;
  std::atomic<bool> ran2 = false;
  std::vector<PolyAction> tasks = {
      [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        ran1 = true;
      },
      [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        ran2 = true;
      }};

  polymorphic_actions.enqueue_bulk(tasks.begin(), tasks.size());
  const auto now = std::chrono::steady_clock::now();
  // 4 ms is not enough to run two 3 ms tasks.. unless of course it does it in
  // parallel!
  while (std::chrono::steady_clock::now() - now <
         std::chrono::milliseconds(4)) {  // loop
  }
  REQUIRE((ran1 && ran2));
  REQUIRE(
      (std::chrono::steady_clock::now() < now + std::chrono::milliseconds(6)));
  // stop the pool.
  stp.stop();
}
