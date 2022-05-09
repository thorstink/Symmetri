#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"
#include "some_ros.hpp"

inline static symmetri::Store getStore() {
  return {{"t0", &T1::action0}, {"t1", &T1::action1}, {"t2", &action2},
          {"t3", &action3},     {"t6", &action4},     {"t5", &action5}};
};

int main(int argc, char *argv[]) {
  ros::init(argc, argv, "rosthingy");

  ros_example::someSub sub;
  ros_example::somePub pub;

  auto pnml_path = std::string(argv[1]);
  auto store = getStore();
  store.insert({"t4", [&]() { return pub.publish(); }});

  auto go = symmetri::start(pnml_path, store);
  auto ros_thread = std::thread([]() { ros::spin(); });

  go();
  ros_thread.join();
}
