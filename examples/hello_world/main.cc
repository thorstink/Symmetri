// This is an example of how to use the petri-net library: In this example we
// have a net where transition t50 can be triggered by inputting the character
// '1' through std::cin (the keyboard). When t50 is triggered, the net becomes
// live. This example has no ' final marking ' (goal marking), so it' ll run
// forever until the user hits ctrl-c.

#include <iostream>
#include <thread>

#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"

// Before main, we define a bunch of functions that can be bound to
// transitions in the petri net.

// this is just a random function taking no arguments and returning
// nothing. Sleeping a bit to make it not instantaneous. Note this
// function doesn't/can not fail: it just returns void (which means success)
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

// If you want to specify 'success' or 'failure' you can return a
// "Token". It can be {Started, Success, Deadlocked, Canceled, Failed} or a user
// defined Token. See next example. and in the case of defining your own
// function it makes sense to return either "Success" or "Failed". The Other
// states re meant when the function is a nested petri-net.
symmetri::Token helloResult() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return symmetri::Color::Success;
}

// Tokens with custom colors can be created like this:
const static symmetri::Token CustomToken(
    symmetri::Color::registerToken("ACustomTokenColor"));

// which can be used as a result like any other ordinary token;
symmetri::Token returnCustomToken() { return CustomToken; }

// The main is simply the body of a cpp program. It has to start somewhere, so
// that's here.
int main(int, char *argv[]) {
  // So here we simply load the arguments and assign them to a string-type
  // variable.
  const std::string petri_net(argv[1]);

  // This is a very simple thread pool. It can be shared among nets.
  auto pool = std::make_shared<symmetri::TaskSystem>(1);

  // This is the construction of the class that executes the functions in the
  // store based on the petri net. You can specify a final marking, the amount
  // of threads it can use (maximum amount of stuff it can do in parallel) and a
  // name so the net is easy to identify in a log.
  symmetri::PetriNet net({petri_net}, "CASE_X", pool);

  auto result =
      fire(net);  // This function blocks until either
                  // the net completes, deadlocks or user requests exit (ctrl-c)

  // this simply prints the event log
  for (const auto &[caseid, t, s, c] : getLog(net)) {
    std::cout << "Eventlog: " << caseid << ", " << t << ", "
              << symmetri::Color::toString(s) << ", "
              << c.time_since_epoch().count() << std::endl;
  }

  // return the result! If everything went well, you typically return
  // 0 as exit code.
  return result == symmetri::Color::Success ? 0 : -1;
}
