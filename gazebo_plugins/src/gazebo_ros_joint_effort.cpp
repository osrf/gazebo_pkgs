// Copyright 2013 Open Source Robotics Foundation, Inc.
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

#include <gazebo/common/Events.hh>
#include <gazebo/common/Time.hh>
#include <gazebo/physics/Joint.hh>
#include <gazebo/physics/Model.hh>
#include <gazebo/physics/World.hh>
#include <gazebo_plugins/gazebo_ros_joint_effort.hpp>
#include <gazebo_ros/conversions/builtin_interfaces.hpp>
#include <gazebo_ros/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include <memory>
#include <string>
#include <vector>

namespace gazebo_plugins
{
class GazeboRosJointEffortPrivate
{
public:
  /// A pointer to the GazeboROS node.
  gazebo_ros::Node::SharedPtr ros_node_;

  /// Joints being tracked.
  std::vector<gazebo::physics::JointPtr> joints_;

  /// Joint state subscriber
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_sub_;

  /// Container for the effort that this plugin exerts on the body.
  sensor_msgs::msg::JointState joint_state_msg_;

  // Pointer to the update event connection
  gazebo::event::ConnectionPtr update_connection_;
};

GazeboRosJointEffort::GazeboRosJointEffort()
: impl_(std::make_unique<GazeboRosJointEffortPrivate>())
{
}

GazeboRosJointEffort::~GazeboRosJointEffort()
{
}

void GazeboRosJointEffort::Load(gazebo::physics::ModelPtr model, sdf::ElementPtr sdf)
{
  // ROS node
  impl_->ros_node_ = gazebo_ros::Node::Get(sdf);

  // Joints
  if (!sdf->HasElement("joint_name")) {
    RCLCPP_ERROR(impl_->ros_node_->get_logger(), "Plugin missing <joint_name>s");
    impl_->ros_node_.reset();
    return;
  }

  sdf::ElementPtr joint_elem = sdf->GetElement("joint_name");
  while (joint_elem) {
    auto joint_name = joint_elem->Get<std::string>();

    auto joint = model->GetJoint(joint_name);
    if (!joint) {
      RCLCPP_ERROR(impl_->ros_node_->get_logger(), "Joint %s does not exist!", joint_name.c_str());
    } else {
      impl_->joints_.push_back(joint);
      RCLCPP_INFO(impl_->ros_node_->get_logger(), "Going to publish joint [%s]",
        joint_name.c_str() );
    }
    joint_elem = joint_elem->GetNextElement("joint_name");
  }

  if (impl_->joints_.empty()) {
    RCLCPP_ERROR(impl_->ros_node_->get_logger(), "No joints found.");
    impl_->ros_node_.reset();
    return;
  }

  impl_->joint_state_sub_ = impl_->ros_node_->create_subscription<sensor_msgs::msg::JointState>(
    "joint_efforts", rclcpp::SystemDefaultsQoS(),
    std::bind(&GazeboRosJointEffort::OnRosJointStateMsg, this, std::placeholders::_1));

  // Callback on every iteration
  impl_->update_connection_ = gazebo::event::Events::ConnectWorldUpdateBegin(
    std::bind(&GazeboRosJointEffort::OnUpdate, this));

  impl_->joint_state_msg_.effort.resize(impl_->joints_.size());
  impl_->joint_state_msg_.position.resize(impl_->joints_.size());
  impl_->joint_state_msg_.velocity.resize(impl_->joints_.size());
}

void GazeboRosJointEffort::OnRosJointStateMsg(const sensor_msgs::msg::JointState::SharedPtr msg)
{
  if (msg->effort.size() != impl_->joints_.size()) {
    RCLCPP_ERROR(
      impl_->ros_node_->get_logger(),
      "Joint input size differs from msg");
  }

  impl_->joint_state_msg_.header = msg->header;
  for (size_t i = 0; i < impl_->joints_.size(); ++i) {
    impl_->joint_state_msg_.effort[i] = msg->effort[i];
  }
}

void GazeboRosJointEffort::OnUpdate()
{
  for (size_t i = 0; i < impl_->joints_.size(); ++i) {
    impl_->joints_[i]->SetForce(0, impl_->joint_state_msg_.effort[i]);
  }
}

GZ_REGISTER_MODEL_PLUGIN(GazeboRosJointEffort)
}  // namespace gazebo_plugins
