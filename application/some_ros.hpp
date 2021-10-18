#pragma once
#include "ros/ros.h"
#include "std_msgs/String.h"

namespace ros_example {

void chatterCallback(const std_msgs::String::ConstPtr &msg) {
  ROS_INFO("I heard: [%s]", msg->data.c_str());
}
struct someSub {
  someSub() { sub = n.subscribe("chatter",  1, chatterCallback); };
  ros::NodeHandle n;
  ros::Subscriber sub;
};
}; // namespace ros_example