/*
 * Copyright 2013 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

/* Desc: External interfaces for Gazebo
 * Author: Nate Koenig, John Hsu, Dave Coleman
 * Date: Jun 10 2013
 */

#include <gazebo/common/Events.hh>
#include <gazebo/gazebo_config.h>
#include <gazebo_ros/gazebo_ros_api_plugin.h>

namespace gazebo
{

GazeboRosApiPlugin::GazeboRosApiPlugin() :
  world_created_(false),
  stop_(false),
  plugin_loaded_(false),
  pub_clock_frequency_(0),
  enable_ros_network_(true)
{
  robot_namespace_.clear();
}

GazeboRosApiPlugin::~GazeboRosApiPlugin()
{
  ROS_DEBUG_STREAM_NAMED("api_plugin","GazeboRosApiPlugin Deconstructor start");

  // Unload the sigint event
  sigint_event_.reset();
  ROS_DEBUG_STREAM_NAMED("api_plugin","After sigint_event unload");

  // Don't attempt to unload this plugin if it was never loaded in the Load() function
  if(!plugin_loaded_)
  {
    ROS_DEBUG_STREAM_NAMED("api_plugin","Deconstructor skipped because never loaded");
    return;
  }

  // Disconnect slots
  load_gazebo_ros_api_plugin_event_.reset();
  wrench_update_event_.reset();
  force_update_event_.reset();
  time_update_event_.reset();
  ROS_DEBUG_STREAM_NAMED("api_plugin","Slots disconnected");

  // Stop the multi threaded ROS spinner
  async_ros_spin_->stop();
  ROS_DEBUG_STREAM_NAMED("api_plugin","Async ROS Spin Stopped");

  // Shutdown the ROS node
  nh_->shutdown();
  ROS_DEBUG_STREAM_NAMED("api_plugin","Node Handle Shutdown");

  // Shutdown ROS queue
  gazebo_callback_queue_thread_->join();
  ROS_DEBUG_STREAM_NAMED("api_plugin","Callback Queue Joined");

  // Delete Force and Wrench Jobs
  lock_.lock();
  for (std::vector<GazeboRosApiPlugin::ForceJointJob*>::iterator iter=force_joint_jobs_.begin();iter!=force_joint_jobs_.end();)
  {
    delete (*iter);
    iter = force_joint_jobs_.erase(iter);
  }
  force_joint_jobs_.clear();
  ROS_DEBUG_STREAM_NAMED("api_plugin","ForceJointJobs deleted");
  for (std::vector<GazeboRosApiPlugin::WrenchBodyJob*>::iterator iter=wrench_body_jobs_.begin();iter!=wrench_body_jobs_.end();)
  {
    delete (*iter);
    iter = wrench_body_jobs_.erase(iter);
  }
  wrench_body_jobs_.clear();
  lock_.unlock();
  ROS_DEBUG_STREAM_NAMED("api_plugin","WrenchBodyJobs deleted");

  ROS_DEBUG_STREAM_NAMED("api_plugin","Unloaded");
}

void GazeboRosApiPlugin::shutdownSignal()
{
  ROS_DEBUG_STREAM_NAMED("api_plugin","shutdownSignal() recieved");
  stop_ = true;
}

void GazeboRosApiPlugin::Load(int argc, char** argv)
{
  ROS_DEBUG_STREAM_NAMED("api_plugin","Load");

  // connect to sigint event
  sigint_event_ = gazebo::event::Events::ConnectSigInt(boost::bind(&GazeboRosApiPlugin::shutdownSignal,this));

  // setup ros related
  if (!ros::isInitialized())
    ros::init(argc,argv,"gazebo",ros::init_options::NoSigintHandler);
  else
    ROS_ERROR_NAMED("api_plugin", "Something other than this gazebo_ros_api plugin started ros::init(...), command line arguments may not be parsed properly.");

  // check if the ros master is available - required
  while(!ros::master::check())
  {
    ROS_WARN_STREAM_NAMED("api_plugin","No ROS master - start roscore to continue...");
    // wait 0.5 second
    usleep(500*1000); // can't use ROS Time here b/c node handle is not yet initialized

    if(stop_)
    {
      ROS_WARN_STREAM_NAMED("api_plugin","Canceled loading Gazebo ROS API plugin by sigint event");
      return;
    }
  }

  nh_.reset(new ros::NodeHandle("~")); // advertise topics and services in this node's namespace

  // Built-in multi-threaded ROS spinning
  async_ros_spin_.reset(new ros::AsyncSpinner(0)); // will use a thread for each CPU core
  async_ros_spin_->start();

  /// \brief setup custom callback queue
  gazebo_callback_queue_thread_.reset(new boost::thread( &GazeboRosApiPlugin::gazeboQueueThread, this) );

  // below needs the world to be created first
  load_gazebo_ros_api_plugin_event_ = gazebo::event::Events::ConnectWorldCreated(boost::bind(&GazeboRosApiPlugin::loadGazeboRosApiPlugin,this,_1));

  nh_->getParam("enable_ros_network", enable_ros_network_);

  plugin_loaded_ = true;
  ROS_INFO_NAMED("api_plugin", "Finished loading Gazebo ROS API Plugin.");
}

void GazeboRosApiPlugin::loadGazeboRosApiPlugin(std::string world_name)
{
  // make sure things are only called once
  lock_.lock();
  if (world_created_)
  {
    lock_.unlock();
    return;
  }

  // set flag to true and load this plugin
  world_created_ = true;
  lock_.unlock();

  world_ = gazebo::physics::get_world(world_name);
  if (!world_)
  {
    //ROS_ERROR_NAMED("api_plugin", "world name: [%s]",world->Name().c_str());
    // connect helper function to signal for scheduling torque/forces, etc
    ROS_FATAL_NAMED("api_plugin", "cannot load gazebo ros api server plugin, physics::get_world() fails to return world");
    return;
  }

  gazebonode_ = gazebo::transport::NodePtr(new gazebo::transport::Node());
  gazebonode_->Init(world_name);
  //stat_sub_ = gazebonode_->Subscribe("~/world_stats", &GazeboRosApiPlugin::publishSimTime, this); // TODO: does not work in server plugin?
  light_modify_pub_ = gazebonode_->Advertise<gazebo::msgs::Light>("~/light/modify");

  /// \brief advertise all services
  if (enable_ros_network_)
    advertiseServices();

  // Manage clock for simulated ros time
  pub_clock_ = nh_->advertise<rosgraph_msgs::Clock>("/clock",10);
  // set param for use_sim_time if not set by user already
  if(!(nh_->hasParam("/use_sim_time")))
    nh_->setParam("/use_sim_time", true);

  nh_->getParam("pub_clock_frequency", pub_clock_frequency_);
#if GAZEBO_MAJOR_VERSION >= 8
  last_pub_clock_time_ = world_->SimTime();
#else
  last_pub_clock_time_ = world_->GetSimTime();
#endif

  // hooks for applying forces, publishing simtime on /clock
  wrench_update_event_ = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboRosApiPlugin::wrenchBodySchedulerSlot,this));
  force_update_event_  = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboRosApiPlugin::forceJointSchedulerSlot,this));
  time_update_event_   = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboRosApiPlugin::publishSimTime,this));
}

void GazeboRosApiPlugin::gazeboQueueThread()
{
  static const double timeout = 0.001;
  while (nh_->ok())
  {
    gazebo_queue_.callAvailable(ros::WallDuration(timeout));
  }
}

void GazeboRosApiPlugin::advertiseServices()
{
  if (! enable_ros_network_)
  {
    ROS_INFO_NAMED("api_plugin", "ROS gazebo topics/services are disabled");
    return;
  }

  pub_clock_ = nh_->advertise<rosgraph_msgs::Clock>("/clock",10);

  // Advertise more services on the custom queue
  std::string set_model_configuration_service_name("set_model_configuration");
  ros::AdvertiseServiceOptions set_model_configuration_aso =
    ros::AdvertiseServiceOptions::create<gazebo_msgs::SetModelConfiguration>(
                                                                             set_model_configuration_service_name,
                                                                             boost::bind(&GazeboRosApiPlugin::setModelConfiguration,this,_1,_2),
                                                                             ros::VoidPtr(), &gazebo_queue_);
  set_model_configuration_service_ = nh_->advertiseService(set_model_configuration_aso);

  // Advertise more services on the custom queue
  std::string apply_body_wrench_service_name("apply_body_wrench");
  ros::AdvertiseServiceOptions apply_body_wrench_aso =
    ros::AdvertiseServiceOptions::create<gazebo_msgs::ApplyBodyWrench>(
                                                                       apply_body_wrench_service_name,
                                                                       boost::bind(&GazeboRosApiPlugin::applyBodyWrench,this,_1,_2),
                                                                       ros::VoidPtr(), &gazebo_queue_);
  apply_body_wrench_service_ = nh_->advertiseService(apply_body_wrench_aso);

  // Advertise more services on the custom queue
  std::string apply_joint_effort_service_name("apply_joint_effort");
  ros::AdvertiseServiceOptions apply_joint_effort_aso =
    ros::AdvertiseServiceOptions::create<gazebo_msgs::ApplyJointEffort>(
                                                                        apply_joint_effort_service_name,
                                                                        boost::bind(&GazeboRosApiPlugin::applyJointEffort,this,_1,_2),
                                                                        ros::VoidPtr(), &gazebo_queue_);
  apply_joint_effort_service_ = nh_->advertiseService(apply_joint_effort_aso);

  // Advertise more services on the custom queue
  std::string clear_joint_forces_service_name("clear_joint_forces");
  ros::AdvertiseServiceOptions clear_joint_forces_aso =
    ros::AdvertiseServiceOptions::create<gazebo_msgs::JointRequest>(
                                                                    clear_joint_forces_service_name,
                                                                    boost::bind(&GazeboRosApiPlugin::clearJointForces,this,_1,_2),
                                                                    ros::VoidPtr(), &gazebo_queue_);
  clear_joint_forces_service_ = nh_->advertiseService(clear_joint_forces_aso);

  // Advertise more services on the custom queue
  std::string clear_body_wrenches_service_name("clear_body_wrenches");
  ros::AdvertiseServiceOptions clear_body_wrenches_aso =
    ros::AdvertiseServiceOptions::create<gazebo_msgs::BodyRequest>(
                                                                   clear_body_wrenches_service_name,
                                                                   boost::bind(&GazeboRosApiPlugin::clearBodyWrenches,this,_1,_2),
                                                                   ros::VoidPtr(), &gazebo_queue_);
  clear_body_wrenches_service_ = nh_->advertiseService(clear_body_wrenches_aso);

  // set param for use_sim_time if not set by user already
  if(!(nh_->hasParam("/use_sim_time")))
    nh_->setParam("/use_sim_time", true);

  // todo: contemplate setting environment variable ROBOT=sim here???
  nh_->getParam("pub_clock_frequency", pub_clock_frequency_);
#if GAZEBO_MAJOR_VERSION >= 8
  last_pub_clock_time_ = world_->SimTime();
#else
  last_pub_clock_time_ = world_->GetSimTime();
#endif
}

bool GazeboRosApiPlugin::applyJointEffort(gazebo_msgs::ApplyJointEffort::Request &req,
                                          gazebo_msgs::ApplyJointEffort::Response &res)
{
  gazebo::physics::JointPtr joint;
#if GAZEBO_MAJOR_VERSION >= 8
  for (unsigned int i = 0; i < world_->ModelCount(); i ++)
  {
    joint = world_->ModelByIndex(i)->GetJoint(req.joint_name);
#else
  for (unsigned int i = 0; i < world_->GetModelCount(); i ++)
  {
    joint = world_->GetModel(i)->GetJoint(req.joint_name);
#endif
    if (joint)
    {
      GazeboRosApiPlugin::ForceJointJob* fjj = new GazeboRosApiPlugin::ForceJointJob;
      fjj->joint = joint;
      fjj->force = req.effort;
      fjj->start_time = req.start_time;
#if GAZEBO_MAJOR_VERSION >= 8
      if (fjj->start_time < ros::Time(world_->SimTime().Double()))
        fjj->start_time = ros::Time(world_->SimTime().Double());
#else
      if (fjj->start_time < ros::Time(world_->GetSimTime().Double()))
        fjj->start_time = ros::Time(world_->GetSimTime().Double());
#endif
      fjj->duration = req.duration;
      lock_.lock();
      force_joint_jobs_.push_back(fjj);
      lock_.unlock();

      res.success = true;
      res.status_message = "ApplyJointEffort: effort set";
      return true;
    }
  }

  res.success = false;
  res.status_message = "ApplyJointEffort: joint not found";
  return true;
}

bool GazeboRosApiPlugin::clearJointForces(gazebo_msgs::JointRequest::Request &req,
                                          gazebo_msgs::JointRequest::Response &res)
{
  return clearJointForces(req.joint_name);
}
bool GazeboRosApiPlugin::clearJointForces(std::string joint_name)
{
  bool search = true;
  lock_.lock();
  while(search)
  {
    search = false;
    for (std::vector<GazeboRosApiPlugin::ForceJointJob*>::iterator iter=force_joint_jobs_.begin();iter!=force_joint_jobs_.end();++iter)
    {
      if ((*iter)->joint->GetName() == joint_name)
      {
        // found one, search through again
        search = true;
        delete (*iter);
        force_joint_jobs_.erase(iter);
        break;
      }
    }
  }
  lock_.unlock();
  return true;
}

bool GazeboRosApiPlugin::clearBodyWrenches(gazebo_msgs::BodyRequest::Request &req,
                                           gazebo_msgs::BodyRequest::Response &res)
{
  return clearBodyWrenches(req.body_name);
}
bool GazeboRosApiPlugin::clearBodyWrenches(std::string body_name)
{
  bool search = true;
  lock_.lock();
  while(search)
  {
    search = false;
    for (std::vector<GazeboRosApiPlugin::WrenchBodyJob*>::iterator iter=wrench_body_jobs_.begin();iter!=wrench_body_jobs_.end();++iter)
    {
      //ROS_ERROR_NAMED("api_plugin", "search %s %s",(*iter)->body->GetScopedName().c_str(), body_name.c_str());
      if ((*iter)->body->GetScopedName() == body_name)
      {
        // found one, search through again
        search = true;
        delete (*iter);
        wrench_body_jobs_.erase(iter);
        break;
      }
    }
  }
  lock_.unlock();
  return true;
}

bool GazeboRosApiPlugin::setModelConfiguration(gazebo_msgs::SetModelConfiguration::Request &req,
                                               gazebo_msgs::SetModelConfiguration::Response &res)
{
  std::string gazebo_model_name = req.model_name;

  // search for model with name
#if GAZEBO_MAJOR_VERSION >= 8
  gazebo::physics::ModelPtr gazebo_model = world_->ModelByName(req.model_name);
#else
  gazebo::physics::ModelPtr gazebo_model = world_->GetModel(req.model_name);
#endif
  if (!gazebo_model)
  {
    ROS_ERROR_NAMED("api_plugin", "SetModelConfiguration: model [%s] does not exist",gazebo_model_name.c_str());
    res.success = false;
    res.status_message = "SetModelConfiguration: model does not exist";
    return true;
  }

  if (req.joint_names.size() == req.joint_positions.size())
  {
    std::map<std::string, double> joint_position_map;
    for (unsigned int i = 0; i < req.joint_names.size(); i++)
    {
      joint_position_map[req.joint_names[i]] = req.joint_positions[i];
    }

    // make the service call to pause gazebo
    bool is_paused = world_->IsPaused();
    if (!is_paused) world_->SetPaused(true);

    gazebo_model->SetJointPositions(joint_position_map);

    // resume paused state before this call
    world_->SetPaused(is_paused);

    res.success = true;
    res.status_message = "SetModelConfiguration: success";
    return true;
  }
  else
  {
    res.success = false;
    res.status_message = "SetModelConfiguration: joint name and position list have different lengths";
    return true;
  }
}

void GazeboRosApiPlugin::transformWrench( ignition::math::Vector3d &target_force, ignition::math::Vector3d &target_torque,
                                          const ignition::math::Vector3d &reference_force,
                                          const ignition::math::Vector3d &reference_torque,
                                          const ignition::math::Pose3d &target_to_reference )
{
  // rotate force into target frame
  target_force = target_to_reference.Rot().RotateVector(reference_force);
  // rotate torque into target frame
  target_torque = target_to_reference.Rot().RotateVector(reference_torque);

  // target force is the refence force rotated by the target->reference transform
  target_torque = target_torque + target_to_reference.Pos().Cross(target_force);
}

bool GazeboRosApiPlugin::applyBodyWrench(gazebo_msgs::ApplyBodyWrench::Request &req,
                                         gazebo_msgs::ApplyBodyWrench::Response &res)
{
#if GAZEBO_MAJOR_VERSION >= 8
  gazebo::physics::LinkPtr body = boost::dynamic_pointer_cast<gazebo::physics::Link>(world_->EntityByName(req.body_name));
  gazebo::physics::EntityPtr frame = world_->EntityByName(req.reference_frame);
#else
  gazebo::physics::LinkPtr body = boost::dynamic_pointer_cast<gazebo::physics::Link>(world_->GetEntity(req.body_name));
  gazebo::physics::EntityPtr frame = world_->GetEntity(req.reference_frame);
#endif
  if (!body)
  {
    ROS_ERROR_NAMED("api_plugin", "ApplyBodyWrench: body [%s] does not exist",req.body_name.c_str());
    res.success = false;
    res.status_message = "ApplyBodyWrench: body does not exist";
    return true;
  }

  // target wrench
  ignition::math::Vector3d reference_force(req.wrench.force.x,req.wrench.force.y,req.wrench.force.z);
  ignition::math::Vector3d reference_torque(req.wrench.torque.x,req.wrench.torque.y,req.wrench.torque.z);
  ignition::math::Vector3d reference_point(req.reference_point.x,req.reference_point.y,req.reference_point.z);

  ignition::math::Vector3d target_force;
  ignition::math::Vector3d target_torque;

  /// shift wrench to body frame if a non-zero reference point is given
  ///   @todo: to be more general, should we make the reference point a reference pose?
  reference_torque = reference_torque + reference_point.Cross(reference_force);

  /// @todo: FIXME map is really wrong, need to use tf here somehow
  if (frame)
  {
    // get reference frame (body/model(body)) pose and
    // transform target pose to absolute world frame
    // @todo: need to modify wrench (target force and torque by frame)
    //        transform wrench from reference_point in reference_frame
    //        into the reference frame of the body
    //        first, translate by reference point to the body frame
#if GAZEBO_MAJOR_VERSION >= 8
    ignition::math::Pose3d framePose = frame->WorldPose();
    ignition::math::Pose3d bodyPose = body->WorldPose();
#else
    ignition::math::Pose3d framePose = frame->GetWorldPose().Ign();
    ignition::math::Pose3d bodyPose = body->GetWorldPose().Ign();
#endif
    ignition::math::Pose3d target_to_reference = framePose - bodyPose;
    ROS_DEBUG_NAMED("api_plugin", "reference frame for applied wrench: [%f %f %f, %f %f %f]-[%f %f %f, %f %f %f]=[%f %f %f, %f %f %f]",
              bodyPose.Pos().X(),
              bodyPose.Pos().Y(),
              bodyPose.Pos().Z(),
              bodyPose.Rot().Euler().X(),
              bodyPose.Rot().Euler().Y(),
              bodyPose.Rot().Euler().Z(),
              framePose.Pos().X(),
              framePose.Pos().Y(),
              framePose.Pos().Z(),
              framePose.Rot().Euler().X(),
              framePose.Rot().Euler().Y(),
              framePose.Rot().Euler().Z(),
              target_to_reference.Pos().X(),
              target_to_reference.Pos().Y(),
              target_to_reference.Pos().Z(),
              target_to_reference.Rot().Euler().X(),
              target_to_reference.Rot().Euler().Y(),
              target_to_reference.Rot().Euler().Z()
              );
    transformWrench(target_force, target_torque, reference_force, reference_torque, target_to_reference);
    ROS_ERROR_NAMED("api_plugin", "wrench defined as [%s]:[%f %f %f, %f %f %f] --> applied as [%s]:[%f %f %f, %f %f %f]",
              frame->GetName().c_str(),
              reference_force.X(),
              reference_force.Y(),
              reference_force.Z(),
              reference_torque.X(),
              reference_torque.Y(),
              reference_torque.Z(),
              body->GetName().c_str(),
              target_force.X(),
              target_force.Y(),
              target_force.Z(),
              target_torque.X(),
              target_torque.Y(),
              target_torque.Z()
              );

  }
  else if (req.reference_frame == "" || req.reference_frame == "world" || req.reference_frame == "map" || req.reference_frame == "/map")
  {
    ROS_INFO_NAMED("api_plugin", "ApplyBodyWrench: reference_frame is empty/world/map, using inertial frame, transferring from body relative to inertial frame");
    // FIXME: transfer to inertial frame
#if GAZEBO_MAJOR_VERSION >= 8
    ignition::math::Pose3d target_to_reference = body->WorldPose();
#else
    ignition::math::Pose3d target_to_reference = body->GetWorldPose().Ign();
#endif
    target_force = reference_force;
    target_torque = reference_torque;

  }
  else
  {
    ROS_ERROR_NAMED("api_plugin", "ApplyBodyWrench: reference_frame is not a valid entity name");
    res.success = false;
    res.status_message = "ApplyBodyWrench: reference_frame not found";
    return true;
  }

  // apply wrench
  // schedule a job to do below at appropriate times:
  // body->SetForce(force)
  // body->SetTorque(torque)
  GazeboRosApiPlugin::WrenchBodyJob* wej = new GazeboRosApiPlugin::WrenchBodyJob;
  wej->body = body;
  wej->force = target_force;
  wej->torque = target_torque;
  wej->start_time = req.start_time;
#if GAZEBO_MAJOR_VERSION >= 8
  if (wej->start_time < ros::Time(world_->SimTime().Double()))
    wej->start_time = ros::Time(world_->SimTime().Double());
#else
  if (wej->start_time < ros::Time(world_->GetSimTime().Double()))
    wej->start_time = ros::Time(world_->GetSimTime().Double());
#endif
  wej->duration = req.duration;
  lock_.lock();
  wrench_body_jobs_.push_back(wej);
  lock_.unlock();

  res.success = true;
  res.status_message = "";
  return true;
}

void GazeboRosApiPlugin::wrenchBodySchedulerSlot()
{
  // MDMutex locks in case model is getting deleted, don't have to do this if we delete jobs first
  // boost::recursive_mutex::scoped_lock lock(*world->GetMDMutex());
  lock_.lock();
  for (std::vector<GazeboRosApiPlugin::WrenchBodyJob*>::iterator iter=wrench_body_jobs_.begin();iter!=wrench_body_jobs_.end();)
  {
    // check times and apply wrench if necessary
#if GAZEBO_MAJOR_VERSION >= 8
    ros::Time simTime = ros::Time(world_->SimTime().Double());
#else
    ros::Time simTime = ros::Time(world_->GetSimTime().Double());
#endif
    if (simTime >= (*iter)->start_time)
      if (simTime <= (*iter)->start_time+(*iter)->duration ||
          (*iter)->duration.toSec() < 0.0)
      {
        if ((*iter)->body) // if body exists
        {
          (*iter)->body->SetForce((*iter)->force);
          (*iter)->body->SetTorque((*iter)->torque);
        }
        else
          (*iter)->duration.fromSec(0.0); // mark for delete
      }

    if (simTime > (*iter)->start_time+(*iter)->duration &&
        (*iter)->duration.toSec() >= 0.0)
    {
      // remove from queue once expires
      delete (*iter);
      iter = wrench_body_jobs_.erase(iter);
    }
    else
      ++iter;
  }
  lock_.unlock();
}

void GazeboRosApiPlugin::forceJointSchedulerSlot()
{
  // MDMutex locks in case model is getting deleted, don't have to do this if we delete jobs first
  // boost::recursive_mutex::scoped_lock lock(*world->GetMDMutex());
  lock_.lock();
  for (std::vector<GazeboRosApiPlugin::ForceJointJob*>::iterator iter=force_joint_jobs_.begin();iter!=force_joint_jobs_.end();)
  {
    // check times and apply force if necessary
#if GAZEBO_MAJOR_VERSION >= 8
    ros::Time simTime = ros::Time(world_->SimTime().Double());
#else
    ros::Time simTime = ros::Time(world_->GetSimTime().Double());
#endif
    if (simTime >= (*iter)->start_time)
      if (simTime <= (*iter)->start_time+(*iter)->duration ||
          (*iter)->duration.toSec() < 0.0)
      {
        if ((*iter)->joint) // if joint exists
          (*iter)->joint->SetForce(0,(*iter)->force);
        else
          (*iter)->duration.fromSec(0.0); // mark for delete
      }

    if (simTime > (*iter)->start_time+(*iter)->duration &&
        (*iter)->duration.toSec() >= 0.0)
    {
      // remove from queue once expires
      iter = force_joint_jobs_.erase(iter);
    }
    else
      ++iter;
  }
  lock_.unlock();
}

void GazeboRosApiPlugin::publishSimTime(const boost::shared_ptr<gazebo::msgs::WorldStatistics const> &msg)
{
  ROS_ERROR_NAMED("api_plugin", "CLOCK2");
#if GAZEBO_MAJOR_VERSION >= 8
  gazebo::common::Time sim_time = world_->SimTime();
#else
  gazebo::common::Time sim_time = world_->GetSimTime();
#endif
  if (pub_clock_frequency_ > 0 && (sim_time - last_pub_clock_time_).Double() < 1.0/pub_clock_frequency_)
    return;

  gazebo::common::Time currentTime = gazebo::msgs::Convert( msg->sim_time() );
  rosgraph_msgs::Clock ros_time_;
  ros_time_.clock.fromSec(currentTime.Double());
  //  publish time to ros
  last_pub_clock_time_ = sim_time;
  pub_clock_.publish(ros_time_);
}
void GazeboRosApiPlugin::publishSimTime()
{
#if GAZEBO_MAJOR_VERSION >= 8
  gazebo::common::Time sim_time = world_->SimTime();
#else
  gazebo::common::Time sim_time = world_->GetSimTime();
#endif
  if (pub_clock_frequency_ > 0 && (sim_time - last_pub_clock_time_).Double() < 1.0/pub_clock_frequency_)
    return;

#if GAZEBO_MAJOR_VERSION >= 8
  gazebo::common::Time currentTime = world_->SimTime();
#else
  gazebo::common::Time currentTime = world_->GetSimTime();
#endif
  rosgraph_msgs::Clock ros_time_;
  ros_time_.clock.fromSec(currentTime.Double());
  //  publish time to ros
  last_pub_clock_time_ = sim_time;
  pub_clock_.publish(ros_time_);
}

// Register this plugin with the simulator
GZ_REGISTER_SYSTEM_PLUGIN(GazeboRosApiPlugin)
}
