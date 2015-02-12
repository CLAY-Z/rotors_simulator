/*
 * Copyright 2015 Fadri Furrer, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Michael Burri, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Mina Kamel, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Janosch Nikolic, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Markus Achtelik, ASL, ETH Zurich, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rotors_control/lee_position_controller_node.h"

#include "rotors_control/parameters_ros.h"

namespace rotors_control {

LeePositionControllerNode::LeePositionControllerNode() {

  lee_position_controller_.InitializeParams();

  InitializeParams();

  ros::NodeHandle nh(namespace_);

  cmd_trajectory_sub_ = nh.subscribe(command_trajectory_sub_topic_, 10,
                                     &LeePositionControllerNode::CommandTrajectoryCallback, this);


  odometry_sub_ = nh.subscribe(odometry_sub_topic_, 10,
                               &LeePositionControllerNode::OdometryCallback, this);

  motor_velocity_reference_pub_ = nh.advertise<mav_msgs::MotorSpeed>(
      motor_velocity_reference_pub_topic_, 10);
}

LeePositionControllerNode::~LeePositionControllerNode() { }

void LeePositionControllerNode::InitializeParams() {
  ros::NodeHandle pnh("~");
  pnh.param<std::string>("robotNamespace", namespace_, kDefaultNamespace);
  pnh.param<std::string>("commandTrajectorySubTopic", command_trajectory_sub_topic_, kDefaultCommandTrajectoryTopic);
  pnh.param<std::string>("odometrySubTopic", odometry_sub_topic_, kDefaultOdometrySubTopic);
  pnh.param<std::string>("motorVelocityCommandPubTopic", motor_velocity_reference_pub_topic_, kDefaultMotorVelocityReferencePubTopic);
  //Read parameters from rosparam.
  GetRosParameter(pnh, "position_gain/x",
                  lee_position_controller_.controller_parameters_.position_gain_.x(),
                  &lee_position_controller_.controller_parameters_.position_gain_.x());
  GetRosParameter(pnh, "position_gain/y",
                  lee_position_controller_.controller_parameters_.position_gain_.y(),
                  &lee_position_controller_.controller_parameters_.position_gain_.y());
  GetRosParameter(pnh, "position_gain/z",
                  lee_position_controller_.controller_parameters_.position_gain_.z(),
                  &lee_position_controller_.controller_parameters_.position_gain_.z());
  GetRosParameter(pnh, "velocity_gain/x",
                  lee_position_controller_.controller_parameters_.velocity_gain_.x(),
                  &lee_position_controller_.controller_parameters_.velocity_gain_.x());
  GetRosParameter(pnh, "velocity_gain/y",
                  lee_position_controller_.controller_parameters_.velocity_gain_.y(),
                  &lee_position_controller_.controller_parameters_.velocity_gain_.y());
  GetRosParameter(pnh, "velocity_gain/z",
                  lee_position_controller_.controller_parameters_.velocity_gain_.z(),
                  &lee_position_controller_.controller_parameters_.velocity_gain_.z());
  GetRosParameter(pnh, "attitude_gain/x",
                  lee_position_controller_.controller_parameters_.attitude_gain_.x(),
                  &lee_position_controller_.controller_parameters_.attitude_gain_.x());
  GetRosParameter(pnh, "attitude_gain/y",
                  lee_position_controller_.controller_parameters_.attitude_gain_.y(),
                  &lee_position_controller_.controller_parameters_.attitude_gain_.y());
  GetRosParameter(pnh, "attitude_gain/z",
                  lee_position_controller_.controller_parameters_.attitude_gain_.z(),
                  &lee_position_controller_.controller_parameters_.attitude_gain_.z());
  GetRosParameter(pnh, "angular_rate_gain/x",
                  lee_position_controller_.controller_parameters_.angular_rate_gain_.x(),
                  &lee_position_controller_.controller_parameters_.angular_rate_gain_.x());
  GetRosParameter(pnh, "angular_rate_gain/y",
                  lee_position_controller_.controller_parameters_.angular_rate_gain_.y(),
                  &lee_position_controller_.controller_parameters_.angular_rate_gain_.y());
  GetRosParameter(pnh, "angular_rate_gain/z",
                  lee_position_controller_.controller_parameters_.angular_rate_gain_.z(),
                  &lee_position_controller_.controller_parameters_.angular_rate_gain_.z());
  GetRosParameter(pnh, "mass",
                  lee_position_controller_.vehicle_parameters_.mass_,
                  &lee_position_controller_.vehicle_parameters_.mass_);
  GetRosParameter(pnh, "inertia/xx",
                  lee_position_controller_.vehicle_parameters_.inertia_(0, 0),
                  &lee_position_controller_.vehicle_parameters_.inertia_(0, 0));
  GetRosParameter(pnh, "inertia/yy",
                  lee_position_controller_.vehicle_parameters_.inertia_(1, 1),
                  &lee_position_controller_.vehicle_parameters_.inertia_(1, 1));
  GetRosParameter(pnh, "inertia/zz",
                  lee_position_controller_.vehicle_parameters_.inertia_(2, 2),
                  &lee_position_controller_.vehicle_parameters_.inertia_(2, 2));

  // Get the rotor configuration.
  std::map<std::string, double> single_rotor;
  std::string rotor_configuration_string = "rotor_configuration/";
  unsigned int i = 0;
  RotorConfiguration rotor_configuration;
  while (pnh.getParam(rotor_configuration_string + std::to_string(i), single_rotor)) {
    Rotor rotor;
    pnh.getParam(rotor_configuration_string + std::to_string(i) + "/angle", rotor.angle);
    pnh.getParam(rotor_configuration_string + std::to_string(i) + "/direction", rotor.direction);
    rotor_configuration.rotors.push_back(rotor);
    ++i;
  }
  lee_position_controller_.vehicle_parameters_.rotor_configuration_ = rotor_configuration;
  lee_position_controller_.UpdateControllerMembers();
}
void LeePositionControllerNode::Publish() {
}

void LeePositionControllerNode::CommandTrajectoryCallback(
    const mav_msgs::CommandTrajectoryConstPtr& trajectory_reference_msg) {
  mav_msgs::EigenCommandTrajectory trajectory;
  mav_msgs::eigenCommandTrajectoryFromMsg(trajectory_reference_msg, &trajectory);
  lee_position_controller_.SetCommandTrajectory(trajectory);
}


void LeePositionControllerNode::OdometryCallback(const nav_msgs::OdometryConstPtr& odometry_msg) {

  ROS_INFO_ONCE("LeePositionController got first odometry message.");

  EigenOdometry odometry;
  eigenOdometryFromMsg(odometry_msg, &odometry);
  lee_position_controller_.SetOdometry(odometry);

  Eigen::VectorXd ref_rotor_velocities;
  lee_position_controller_.CalculateRotorVelocities(&ref_rotor_velocities);

  // Todo(ffurrer): Do this in the conversions header.
  mav_msgs::MotorSpeed turning_velocities_msg;

  turning_velocities_msg.motor_speed.clear();
  for (int i = 0; i < ref_rotor_velocities.size(); i++)
    turning_velocities_msg.motor_speed.push_back(ref_rotor_velocities[i]);
  turning_velocities_msg.header.stamp = odometry_msg->header.stamp;

  motor_velocity_reference_pub_.publish(turning_velocities_msg);
}

}

int main(int argc, char** argv) {
  ros::init(argc, argv, "lee_position_controller_node");

  rotors_control::LeePositionControllerNode lee_position_controller_node;

  ros::spin();

  return 0;
}
