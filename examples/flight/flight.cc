#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

inline static symmetri::TransitionActionMap getStore() {
  return {{"t0", &T1::action0}, {"t1", &T1::action1}, {"t2", &action2},
          {"t3", &action3},     {"t4", &action4},     {"t6", &action4},
          {"t5", &action5}};
};

int main(int argc, char *argv[]) {
  auto pnml_path = std::string(argv[1]);
  auto store = getStore();

  auto go = symmetri::start(pnml_path, store);

  go();
}
