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

#ifndef GAZEBO_ROS__NODE_HPP_
#define GAZEBO_ROS__NODE_HPP_

#include <rclcpp/rclcpp.hpp>

#include <gazebo_ros/executor.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace gazebo_ros
{
/// ROS Node for gazebo plugins
/**
 * \class Node node.hpp <gazebo_ros/node.hpp>
 * Wrapper around an rclcpp::Node which ensures all instances share an executor.
 * \include gazebo_ros/node.hpp
 */
class Node : public rclcpp::Node
{
public:
  /// Shared pointer to a #gazebo_ros::Node
  using SharedPtr = std::shared_ptr<Node>;

  /// Destructor
  virtual ~Node();

  /// Create a #gazebo_ros::Node and add it to the global #gazebo_ros::Executor.
  /**
   * \details This will call rclcpp::init if it hasn't been called yet.
   * \param[in] node_name Name for the new node to create
   * \return A shared pointer to a new #gazebo_ros::Node
   */
  static SharedPtr Create(const std::string & node_name);

  /// Create a #gazebo_ros::Node and add it to the global #gazebo_ros::Executor.
  /**
   * \details This will call rclcpp::init if it hasn't been called yet.
   * \details Sets namespace, remappings, and parameters from SDF.
   * SDF is in the form:
   * \code{.xml}
   * <!-- Optional configurations for a plugin's Node -->
   * <ros>
   *  <!-- Name of node, only use if plugin is one node -->
   *  <node_name>my_node</node_name>
   *  <!-- Namespace of the node -->
   *  <namespace>/my_ns</namespace>
   *  <!-- Command line arguments sent to Node's constructor for remappings -->
   *  <argument>my_topic:=new_topic</argument>
   *  <argument>__name:=super_cool_node</argument>
   *  <!-- Initial ROS params set for node -->
   *  <parameter name="max_velocity" type="int">55</parameter>
   *  <parameter name="publish_odom" type="bool">True</parameter>
   * </ros>
   * \endcode
   * \param[in] node_name Name of node to create
   * \param[in] _sdf An SDF element in the style above or containing a <ros> tag in the style above
   * \return A shared pointer to a new #gazebo_ros::Node
   */
  static SharedPtr Create(const std::string & node_name, sdf::ElementPtr _sdf);

  /// Create a #gazebo_ros::Node and add it to the global #gazebo_ros::Executor.
  /**
   * \details This will call rclcpp::init if it hasn't been called yet.
   * \details Forwards arguments to the constructor for rclcpp::Node
   * \param[in] args List of arguments to pass to <a href="http://docs.ros2.org/latest/api/rclcpp/classrclcpp_1_1_node.html">rclcpp::Node</a>
   * \return A shared pointer to a new #gazebo_ros::Node
   */
  template<typename ... Args>
  static SharedPtr CreateWithArgs(Args && ... args);

  /// Convert an sdf element to an rclcpp::Parameter
  /* \details SDF must have a type and name attribute
   *          where type is either 'int', 'float', 'double','bool' or 'string'.
   *          Note that 'float' and 'double' are both stored as double.
   * Examples:
   * \code{.xml}
   * <parameter type="int" name="my_int">55</parameter>
   * <parameter type="bool" name="my_bool">false</parameter>
   * <parameter type="double" name="my_double">13.37</parameter>
   * <parameter type="float" name="my_float">41.18</parameter>
   * <parameter type="string" name="my_string">Hello World</parameter>
   * \endcode
   * \param[in] _sdf An sdf element in the style mentioned above
   * \return The ROS parameter with the same name, type, and value as the sdf element
   *         On failure, the return parameter.get_type() == rclcpp::ParameterType::PARAMETER_NOT_SET
   */
  static rclcpp::Parameter sdf_to_ros_parameter(sdf::ElementPtr const & _sdf);

private:
  /// Inherit constructor
  using rclcpp::Node::Node;

  /// Points to #static_executor_, so that when all #gazebo_ros::Node instances are destroyed, the
  /// executor thread is too
  std::shared_ptr<Executor> executor_;

  /// Locks #initialized_ and #executor_
  static std::mutex lock_;

  /// Points to an #gazebo_ros::Executor shared between all #gazebo_ros::Node instances
  static std::weak_ptr<Executor> static_executor_;

  /// Gets a logger to log information internal to the node
  static rclcpp::Logger internal_logger();
};

template<typename ... Args>
Node::SharedPtr Node::CreateWithArgs(Args && ... args)
{
  std::lock_guard<std::mutex> l(lock_);

  // Contruct Node by forwarding arguments
  // TODO(chapulina): use rclcpp::is_initialized() once that's available, see
  // https://github.com/ros2/rclcpp/issues/518
  Node::SharedPtr node;
  try {
    node = std::make_shared<Node>(std::forward<Args>(args) ...);
  } catch (rclcpp::exceptions::RCLError e) {
    // If node construction failed because ROS in not initialized,
    // initialize ROS with no arguments and try again.
    if (RCL_RET_NOT_INIT == e.ret) {
      rclcpp::init(0, nullptr);
      RCLCPP_INFO(internal_logger(),
        "ROS was initialized without arguments.");
      node = std::make_shared<Node>(std::forward<Args>(args) ...);
    } else {
      // For all other errors, pass exception up stack
      RCLCPP_ERROR(internal_logger(),
        "Failed to create node. Error: %s", e.what());
      throw e;
    }
  }

  // Store shared pointer to static executor in this object
  node->executor_ = static_executor_.lock();

  // If executor has not been contructed yet, do so now
  if (!node->executor_) {
    node->executor_ = std::make_shared<Executor>();
    static_executor_ = node->executor_;
  }

  // Add new node to the executor so its callbacks are called
  node->executor_->add_node(node);

  return node;
}
}  // namespace gazebo_ros
#endif  // GAZEBO_ROS__NODE_HPP_
