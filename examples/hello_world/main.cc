// This is an example of how to use the petri-net library: In this example we
// have a net where transition t50 can be triggered by inputing the character
// '1' through std::cin (the keyboard). When t50 is triggered, the net becomes
// live. This example has no ' final marking ' (goal marking), so it' ll run
// forever until the user hits ctrl-c.
#include <spdlog/spdlog.h>

#include <iostream>
#include <thread>

#include "symmetri/symmetri.h"

// Before main, we define a bunch of functions that can be bound to
// transitions in the petri net.

// this is just a random function taking no arguments and returning
// nothing. Sleeping a bit to make it not instantaneous. Note this
// function doesn't/can not fail: it just returns void (which means success)
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

// If you want to specify 'success' or 'failure' you can return a
// "TransitionState". It can be {Started, Completed, Deadlock, UserExit, Error},
// and in the case of defining your own function it makes sense to return either
// "Completed" or "Error". The Other states re meant when the function is a
// nested petri-net.
symmetri::TransitionState helloResult() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return symmetri::TransitionState::Completed;
}

// The main is simply the body of a cpp program. It has to start somewhere, so
// that's here.
int main(int, char *argv[]) {
  // Through argc and argv you can gather input arguments. E.g. when you launch
  // this petri net application you execute something like
  // "./Symmetri_hello_world ../nets/passive_n1.pnml ../nets/T50startP0.pnml"
  // from the command line. argv[1] binds to ../nets/passive_n1.pnml, etc.

  // So here we simply load the arguments and assign them to a string-type
  // variable.
  const std::string pnml_path_start(argv[1]);
  const std::string pnml_path_passive(argv[2]);

  // Then we have to create a store (I might change the name). But it is
  // essentialy a dictonairy that maps the  transition-ids from the PNML to
  // actual cpp-functions.
  const symmetri::Store store{{"t0", &helloResult}, {"t1", &helloWorld},
                              {"t2", &helloWorld},  {"t3", &helloWorld},
                              {"t4", &helloWorld},  {"t50", &helloWorld}};

  // This is a very simple thread pool. It can be shared among nets.
  symmetri::StoppablePool pool(1);

  // This is the construction of the class that executes the functions in the
  // store based on the petri net. You can specifiy a final marking, the amount
  // of threads it can use (maximum amount of stuff it can do in parallel) and a
  // name so the net is easy to identifiy in a log.
  symmetri::Application net({pnml_path_start, pnml_path_passive}, {}, store, {},
                            "CASE_X", pool);

  // We use a simple boolean flag to terminate the threads once the net
  // finishes. Without it, these threads would prevent the program from cleanly
  // finishing.
  std::atomic<bool> running(true);
  // We create a little thread that listens to events that allow us to manually
  // trigger transitions. E.g. collision avoidance or manual take-over
  std::thread input_thread(
      [&running, t50 = net.registerTransitionCallback("t50")] {
        char input_char;
        do {
          // cin is a way to listen to the keyboard in c++. It hangs until the
          // user hits [some keys] followed by [enter]. The switch-case triggers
          // t50 if the character is 1 and does nothing otherwise.
          std::cin >> input_char;
          switch (input_char) {
            case '1': {
              t50();
              break;
            }
            default:
              spdlog::info("unused character");
          }
        } while (running.load());  // this exits the loop once the running-flag
                                   // becomes false.
      });

  auto [el, result] =
      net();  // This function blocks until either the net completes, deadlocks
  // or user requests exit (ctrl-c)
  running.store(false);  // We set this to false so the thread that we launched
                         // gets interrupted.
  input_thread.join();

  // this simply prints the event log
  uint64_t oldt = 0;
  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
    spdlog::info("{0}", c.time_since_epoch().count() - oldt);
    oldt = c.time_since_epoch().count();
  }

  // return the result! in linux is enverything went well, you typically return
  // 0.
  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
