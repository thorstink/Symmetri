#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>

#include "symmetri/parsers.h"
#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"
#include "transition.hpp"

using namespace symmetri;
/**
 * @brief We want to use the Foo class with Symmetri; Foo has nice
 * functionalities such as Pause and Resume and it can also get
 * preempted/cancelled. We need to define functions to let Symmetri use these
 * functionalities. It is as simple by creating specialized version of the
 * fire/cancel/pause/resume functions. One does not need to implement
 * all - if nothing is defined, a default version is used.
 *
 */

const static Token FooFail(Color::registerToken("FooFail"));

Token fire(const Foo &f) { return f.fire() ? FooFail : Color::Success; }

void cancel(const Foo &f) { f.cancel(); }

void pause(const Foo &f) { f.pause(); }

void resume(const Foo &f) { f.resume(); }

/**
 * @brief A simple printer for the eventlog
 *
 * @param eventlog
 */
void printLog(const symmetri::Eventlog &eventlog) {
  for (const auto &[caseid, t, s, c] : eventlog) {
    spdlog::info("EventLog: {0}, {1}, {2}, {3}", caseid, t, Color::toString(s),
                 c.time_since_epoch().count());
  }
}

void writeMermaidHtmlToFile(const std::string &mermaid) {
  std::ofstream mermaid_file;
  mermaid_file.open("examples/flight/mermaid.html");
  mermaid_file << "<div class=\"mermaid\">" + mermaid + "</div>";
  mermaid_file.close();

  return;
}

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  // We get some paths to PNML-files
  const auto pnml1 = std::string(argv[1]);
  const auto pnml2 = std::string(argv[2]);
  const auto pnml3 = std::string(argv[3]);

  // We create a threadpool to which transitions can be dispatched. In this
  // case 4 is too many; in theory you can deduce the maximum amount of
  // parallel transitions from your petri net.
  auto pool = std::make_shared<symmetri::TaskSystem>(4);

  // Here we create the first PetriNet based on composing pnml1 and pnml2
  // using flat composition. The associated transitions are two instance of
  // the Foo-class.
  PetriNet subnet({pnml1, pnml2}, {{"P2", Color::toString(Color::Success)}}, {},
                  "SubNet", pool);
  subnet.registerTransitionCallback("T0", Foo("SubFoo"));
  subnet.registerTransitionCallback("T1", Foo("SubBar"));

  // We create another PetriNet by flatly composing all three petri nets.
  // Again we have 2 Foo-transitions, and the first transition (T0) is the
  // subnet. This show how you can also nest PetriNets.
  PetriNet bignet({pnml1, pnml2, pnml3},
                  {{"P3", Color::toString(Color::Success)},
                   {"P3", Color::toString(Color::Success)},
                   {"P3", Color::toString(Color::Success)},
                   {"P3", Color::toString(Color::Success)},
                   {"P3", Color::toString(Color::Success)}},
                  "RootNet", pool);
  bignet.registerTransitionCallback("T0", subnet);
  bignet.registerTransitionCallback("T1", Foo("Bar"));
  bignet.registerTransitionCallback("T2", Foo("Foo"));
  // a flag to check if we are running
  std::atomic<bool> running(true);

  // a thread that polls the eventlog and writes it to a file
  auto gantt = std::thread([&] {
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      if (!running) {
        break;
      }
      // writeMermaidHtmlToFile(symmetri::mermaidFromEventlog(getLog(bignet)));
    }
  });

  // Parallel to the PetriNet execution, we run a thread through which we
  // can get some keyboard input for interaction
  auto t = std::thread([&] {
    while (running) {
      std::cout << "input options: \n [p] - pause\n [r] - resume\n [x] - "
                   "exit\n";
      char input;
      std::cin >> input;
      switch (input) {
        case 'p':
          pause(bignet);
          break;
        case 'r':
          resume(bignet);
          break;
        case 'x': {
          cancel(bignet);
          running = false;
          break;
        }
        default:
          break;
      }
      std::cin.get();
    };
  });

  // this is where we call the blocking fire-function that executes the petri
  // net
  auto result = fire(bignet);
  running = false;
  // print the results and eventlog
  spdlog::info("Token of this net: {0}", Color::toString(result));
  const auto el = getLog(bignet);
  auto marking = bignet.getMarking();
  printLog(el);
  gantt.join();  // clean up
  t.join();      // clean up
  // writeMermaidHtmlToFile(symmetri::mermaidFromEventlog(el));
  return 0;
}
