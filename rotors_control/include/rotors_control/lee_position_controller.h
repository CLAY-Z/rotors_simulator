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

#ifndef ROTORS_CONTROL_LEE_POSITION_CONTROLLER_H
#define ROTORS_CONTROL_LEE_POSITION_CONTROLLER_H

#include <mav_msgs/conversions.h>
#include <mav_msgs/eigen_mav_msgs.h>

#include "rotors_control/common.h"
#include "rotors_control/parameters.h"

namespace rotors_control {

class ControllerParameters {
 public:
  ControllerParameters() {};
  void SetParameters(ControllerParameters controller_parameters);
  void GetParameters();

  Eigen::Matrix4Xd allocation_matrix_;
  Eigen::Vector3d position_gain_;
  Eigen::Vector3d velocity_gain_;
  Eigen::Vector3d attitude_gain_;
  Eigen::Vector3d angular_rate_gain_;
  Eigen::Matrix3d inertia_matrix_;
  RotorConfiguration rotor_configuration_;
};

class LeePositionController {
 public:
  LeePositionController();
  ~LeePositionController();
  void InitializeParams();
  void UpdateControllerMembers();
  void CalculateRotorVelocities(Eigen::VectorXd* rotor_velocities) const;

  void SetOdometry(const EigenOdometry& odometry);
  void SetCommandTrajectory(const mav_msgs::EigenCommandTrajectory& command_trajectory);

  ControllerParameters controller_parameters_;
  VehicleParameters vehicle_parameters_;
  PhysicsParameters physics_parameters_;

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
 private:
  bool initialized_params_;

  Eigen::Vector3d normalized_attitude_gain_;
  Eigen::Vector3d normalized_angular_rate_gain_;
  Eigen::MatrixX4d angular_acc_to_rotor_velocities_;

  mav_msgs::EigenCommandTrajectory command_trajectory_;
  EigenOdometry odometry_;

  void ComputeDesiredAngularAcc(const Eigen::Vector3d& acceleration, Eigen::Vector3d* angular_acceleration) const;
  void ComputeDesiredAcceleration(Eigen::Vector3d* acceleration) const;


};
}

#endif // ROTORS_CONTROL_LEE_POSITION_CONTROLLER_H
