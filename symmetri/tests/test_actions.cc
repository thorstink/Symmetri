#include <catch2/catch_test_macros.hpp>

#include "symmetri/actions.h"
#include "symmetri/symmetri.h"

using namespace symmetri;
TEST_CASE("Run the executor") {
  // Create a simple stoppable threadpool with 1 thread.
  auto stp = createStoppablePool(1);
  // launch a task
  std::atomic<bool> ran(false);
  stp->enqueue([&]() { ran.store(true); });
  // wait a little before shutting the thread pool down.
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  REQUIRE(ran);
  // stop the pool.
  stp->stop();
}

TEST_CASE("Run the executor parallel tasks") {
  // Create a simple stoppable threadpool with 2 threads.
  auto stp = createStoppablePool(2);

  // launch a task
  std::atomic<bool> ran1(false);
  std::atomic<bool> ran2(false);
  std::thread::id thread_id1, thread_id2;
  const std::thread::id main_thread(std::this_thread::get_id());

  stp->enqueue([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    thread_id1 = std::this_thread::get_id();
    ran1.store(true);
  });
  stp->enqueue([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    thread_id2 = std::this_thread::get_id();
    ran2.store(true);
  });

  // block until both tasks finished
  while (!ran1.load() || !ran2.load()) {
  }
  REQUIRE((ran1.load() && ran2.load()));
  REQUIRE_FALSE(thread_id1 == thread_id2);
  REQUIRE_FALSE(main_thread == thread_id2);
  REQUIRE_FALSE(thread_id1 == main_thread);
  // stop the pool.
  stp->stop();
}
