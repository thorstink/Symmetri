#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

inline static symmetri::TransitionActionMap getStore() {
  return {{"t0", &T1::action0}, {"t1", &T1::action1}, {"t2", &action3},
          {"t3", &action3},     {"t4", &action4},     {"t6", &action4},
          {"t5", &action5}};
};

int main(int argc, char *argv[]) {
  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);
  auto store = getStore();

  auto go = symmetri::start({pnml1, pnml2, pnml3}, store);

  go();
}
