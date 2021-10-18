#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"
#include "some_ros.hpp"

inline static symmetri::TransitionActionMap getStore() {
  return {{"t0", &T1::action0}, {"t1", &T1::action1}, {"t2", &action2},
          {"t3", &action3},     {"t4", &action4},     {"t5", &action5},
          {"t6", &action6}};
};

int main(int argc, char *argv[]) {
  ros::init(argc, argv, "things");

  ros_example::someSub sub;

  auto pnml_path = std::string(argv[1]);
  auto store = getStore();

  auto go = symmetri::start(pnml_path, store);

  go();
}
