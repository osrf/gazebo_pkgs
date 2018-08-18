// Copyright 2018 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <gazebo_ros/testing_utils.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rosgraph_msgs/msg/clock.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// Tests the simtime published to /clock by gazebo_ros_init
class TestSimTime : public ::testing::Test
{
public:
  TestSimTime() {}
  void SetUp() override;
  void TearDown() override;

protected:
  std::unique_ptr<gazebo_ros::GazeboProcess> gazebo_process_;
};

void TestSimTime::SetUp()
{
  gazebo_process_ = std::make_unique<gazebo_ros::GazeboProcess>(std::vector<const char *>{"-s",
        "libgazebo_ros_init.so"});
  ASSERT_GT(gazebo_process_->Run(), 0);
}

void TestSimTime::TearDown()
{
  ASSERT_GE(gazebo_process_->Terminate(), 0);
  gazebo_process_.reset();
}

TEST_F(TestSimTime, TestClock)
{
  auto node = std::make_shared<rclcpp::Node>("my_node");
  rclcpp::Clock clock;

  using ClockMsg = rosgraph_msgs::msg::Clock;

  auto first_msg =
    gazebo_ros::get_message_or_timeout<ClockMsg>(node, "/clock", rclcpp::Duration(5, 0));
  ASSERT_NE(first_msg, nullptr);
  auto first_msg_time = clock.now();

  auto second_msg =
    gazebo_ros::get_message_or_timeout<ClockMsg>(node, "/clock", rclcpp::Duration(5, 0));
  ASSERT_NE(second_msg, nullptr);
  auto second_msg_time = clock.now();

  ASSERT_GT(second_msg, first_msg);
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
