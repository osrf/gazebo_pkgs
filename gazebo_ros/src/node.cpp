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

#include <gazebo_ros/node.hpp>
#include <sdf/Param.hh>

#include <memory>
#include <string>
#include <vector>

namespace gazebo_ros
{

std::weak_ptr<Executor> Node::static_executor_;
std::mutex Node::lock_;

Node::~Node()
{
}

Node::SharedPtr Node::Get(sdf::ElementPtr sdf)
{
  // Initialize arguments
  std::string name = "";
  std::string ns = "";
  std::vector<std::string> arguments;
  std::vector<rclcpp::Parameter> initial_parameters;

  // Get the name of the plugin as the name for the node.
  if (!sdf->HasAttribute("name")) {
    RCLCPP_WARN(internal_logger(), "Name of plugin not found.");
  }
  name = sdf->Get<std::string>("name");
  // Get inner <ros> element if full plugin sdf was passed in
  if (sdf->HasElement("ros")) {
    sdf = sdf->GetElement("ros");
  }

  // Set namespace if tag is present
  if (sdf->HasElement("namespace")) {
    ns = sdf->GetElement("namespace")->Get<std::string>();
  }

  // Get list of arguments from SDF
  if (sdf->HasElement("argument")) {
    sdf::ElementPtr argument_sdf = sdf->GetElement("argument");

    while (argument_sdf) {
      std::string argument = argument_sdf->Get<std::string>();
      arguments.push_back(argument);
      argument_sdf = argument_sdf->GetNextElement("argument");
    }
  }

  // Convert each parameter tag to a ROS parameter
  if (sdf->HasElement("parameter")) {
    sdf::ElementPtr parameter_sdf = sdf->GetElement("parameter");
    while (parameter_sdf) {
      auto param = sdf_to_ros_parameter(parameter_sdf);
      if (rclcpp::ParameterType::PARAMETER_NOT_SET != param.get_type()) {
        initial_parameters.push_back(param);
      }
      parameter_sdf = parameter_sdf->GetNextElement("parameter");
    }
  }

  // Use default context
  auto context = rclcpp::contexts::default_context::get_global_default_context();

  // Create node with parsed arguments
  return CreateWithArgs(
    name, ns,
    context,
    arguments, initial_parameters);
}

Node::SharedPtr Node::Get()
{
  // TODO(dhood): don't create a new node each call.
  return CreateWithArgs("gazebo");
}

rclcpp::Parameter Node::sdf_to_ros_parameter(sdf::ElementPtr const & sdf)
{
  if (!sdf->HasAttribute("name")) {
    RCLCPP_WARN(internal_logger(),
      "Ignoring parameter because it has no attribute 'name'. Tag: %s", sdf->ToString("").c_str());
    return rclcpp::Parameter();
  }
  if (!sdf->HasAttribute("type")) {
    RCLCPP_WARN(internal_logger(),
      "Ignoring parameter because it has no attribute 'type'. Tag: %s", sdf->ToString("").c_str());
    return rclcpp::Parameter();
  }

  std::string name = sdf->Get<std::string>("name");
  std::string type = sdf->Get<std::string>("type");

  if ("int" == type) {
    return rclcpp::Parameter(name, sdf->Get<int>());
  } else if ("double" == type || "float" == type) {
    return rclcpp::Parameter(name, sdf->Get<double>());
  } else if ("bool" == type) {
    return rclcpp::Parameter(name, sdf->Get<bool>());
  } else if ("string" == type) {
    return rclcpp::Parameter(name, sdf->Get<std::string>());
  } else {
    RCLCPP_WARN(internal_logger(),
      "Ignoring parameter because attribute 'type' is invalid. Tag: %s", sdf->ToString("").c_str());
    return rclcpp::Parameter();
  }
}

rclcpp::Logger Node::internal_logger()
{
  return rclcpp::get_logger("gazebo_ros_node");
}

}  // namespace gazebo_ros
