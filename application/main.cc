#include "Symmetri/symmetri.h"
#include "transitions.h"

// #include "ros/ros.h"
// struct someSub {
//   someSub(const 
//   ros::NodeHandle n;
//   ros::Subscriber sub; =
//       n.subscribe("chatter", 1000, [](const std_msgs::String::ConstPtr &msg) {
//     ROS_INFO("I heard: [%s]", msg->data.c_str());
//       });
// };

int main(int argc, const char *argv[]) {
  // ros::init(argc, argv, "things");
  auto pnml_path = std::string(argv[1]);
  auto store = getStore();

  auto go = symmetri::start(pnml_path, store);

  go();
}
