#pragma once
#include <ros/ros.h>
#include <std_msgs/String.h>

#include <chrono>
#include <thread>

#include "types.h"

namespace ros_example {

void chatterCallback(const std_msgs::String::ConstPtr &msg) {
  ROS_INFO("I heard: [%s]", msg->data.c_str());
}
struct someSub {
  someSub() { sub = n.subscribe("chatter", 1, chatterCallback); };
  ros::NodeHandle n;
  ros::Subscriber sub;
};

struct somePub {
  somePub() { pub = n.advertise<std_msgs::String>("other_chatter", 1000); };
  ros::NodeHandle n;
  ros::Publisher pub;
  void publish() {
    std::cout << "Executing Transition 4 on thread "
              << std::this_thread::get_id() << '\n';
    auto dur = 1800 + 200 * 5 + std::rand() / ((RAND_MAX + 1500u) / 1500);
    std::this_thread::sleep_for(std::chrono::milliseconds(dur));

    std_msgs::String msg;
    msg.data = "hello world";
    pub.publish(msg);
    return std::nullopt;
  }
};
};  // namespace ros_example
