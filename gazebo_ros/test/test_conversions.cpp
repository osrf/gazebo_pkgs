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

#include <gazebo_ros/conversions.hpp>
#include <gtest/gtest.h>

TEST(TestConversions, Vector3)
{
  // Ign to ROS
  ignition::math::Vector3d vec(1.0, 2.0, 3.0);
  auto msg = gazebo_ros::Convert<geometry_msgs::msg::Vector3>(vec);
  EXPECT_EQ(1.0, msg.x);
  EXPECT_EQ(2.0, msg.y);
  EXPECT_EQ(3.0, msg.z);
  // ROS to Ign
  vec = gazebo_ros::Convert<ignition::math::Vector3d>(msg);
  EXPECT_EQ(1.0, vec.X());
  EXPECT_EQ(2.0, vec.Y());
  EXPECT_EQ(3.0, vec.Z());
}

TEST(TestConversions, Quaternion)
{
  // Ign to ROS
  ignition::math::Quaterniond quat(1.0, 0.2, 0.4, 0.6);
  auto quat_msg = gazebo_ros::Convert<geometry_msgs::msg::Quaternion>(quat);
  EXPECT_EQ(0.2, quat_msg.x);
  EXPECT_EQ(0.4, quat_msg.y);
  EXPECT_EQ(0.6, quat_msg.z);
  EXPECT_EQ(1.0, quat_msg.w);
}

TEST(TestConversions, Time)
{
  // Time conversions
  // Gazebo to rclcpp
  gazebo::common::Time time(200, 100);
  auto rostime = gazebo_ros::Convert<rclcpp::Time>(time);
  EXPECT_EQ(200E9 + 100u, rostime.nanoseconds());

  /// Gazebo to ros message
  auto time_msg = gazebo_ros::Convert<builtin_interfaces::msg::Time>(time);
  EXPECT_EQ(200, time_msg.sec);
  EXPECT_EQ(100u, time_msg.nanosec);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
