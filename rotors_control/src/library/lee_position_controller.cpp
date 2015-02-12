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

#include "rotors_control/lee_position_controller.h"

namespace rotors_control {

LeePositionController::LeePositionController()
    : initialized_params_(false) {
}

LeePositionController::~LeePositionController() {
}

void LeePositionController::InitializeParams() {

  controller_parameters_.position_gain_ = Eigen::Vector3d(6, 6, 6);
  controller_parameters_.velocity_gain_ = Eigen::Vector3d(4.7, 4.7, 4.7);
  controller_parameters_.attitude_gain_ = Eigen::Vector3d(3, 3, 0.035);
  controller_parameters_.angular_rate_gain_ = Eigen::Vector3d(0.52, 0.52, 0.025);

  vehicle_parameters_.inertia_ << 0.0347563,  0,  0,
                                  0,  0.0458929,  0,
                                  0,  0, 0.0977;

  vehicle_parameters_.rotor_force_constant_ = 8.54858e-6;  //F_i = k_n * rotor_velocity_i^2

  vehicle_parameters_.rotor_moment_constant_ = 1.6e-2;  // M_i = k_m * F_i
  vehicle_parameters_.arm_length_ = 0.215;
  vehicle_parameters_.mass_ = 1.56779;

  Rotor rotor0, rotor1, rotor2, rotor3, rotor4, rotor5;
  rotor0.angle = 0.52359877559;
  // rotor0.arm_length = 0.215;
  rotor0.direction = 1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor0);
  rotor1.angle = 1.57079632679;
  // rotor0.arm_length = 0.215;
  rotor1.direction = -1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor1);
  rotor2.angle = 2.61799387799;
  // rotor0.arm_length = 0.215;
  rotor2.direction = 1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor2);
  rotor3.angle = -2.61799387799;
  // rotor0.arm_length = 0.215;
  rotor3.direction = -1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor3);
  rotor4.angle = -1.57079632679;
  // rotor0.arm_length = 0.215;
  rotor4.direction = 1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor4);
  rotor5.angle = -0.52359877559;
  // rotor0.arm_length = 0.215;
  rotor5.direction = -1;
  vehicle_parameters_.rotor_configuration_.rotors.push_back(rotor5);

  UpdateControllerMembers();
}

void LeePositionController::UpdateControllerMembers() {
  calculateAllocationMatrix(vehicle_parameters_.rotor_configuration_, &(controller_parameters_.allocation_matrix_));
  // To make the tuning independent of the inertia matrix we divide here.
  normalized_attitude_gain_ = controller_parameters_.attitude_gain_.transpose()
      * vehicle_parameters_.inertia_.inverse();
  // To make the tuning independent of the inertia matrix we divide here.
  normalized_angular_rate_gain_ = controller_parameters_.angular_rate_gain_.transpose()
      * vehicle_parameters_.inertia_.inverse();


  Eigen::Matrix4d K;
  Eigen::Vector4d K_diag;
  K_diag << vehicle_parameters_.arm_length_ * vehicle_parameters_.rotor_force_constant_,
            vehicle_parameters_.arm_length_ * vehicle_parameters_.rotor_force_constant_,
            vehicle_parameters_.rotor_force_constant_ * vehicle_parameters_.rotor_moment_constant_,
            vehicle_parameters_.rotor_force_constant_;

  K << Eigen::Matrix4d(K_diag.asDiagonal());

  Eigen::Matrix4d I;
  I.setZero();
  I.block<3, 3>(0, 0) = vehicle_parameters_.inertia_;
  I(3, 3) = 1;
  angular_acc_to_rotor_velocities_.resize(vehicle_parameters_.rotor_configuration_.rotors.size(), 4);
  angular_acc_to_rotor_velocities_ = controller_parameters_.allocation_matrix_.transpose()
      * (controller_parameters_.allocation_matrix_
      * controller_parameters_.allocation_matrix_.transpose()).inverse() * K.inverse() * I;

  initialized_params_ = true;
}

void LeePositionController::CalculateRotorVelocities(Eigen::VectorXd* rotor_velocities) const {
  assert(rotor_velocities);
  assert(initialized_params_);

  rotor_velocities->resize(vehicle_parameters_.rotor_configuration_.rotors.size());

  Eigen::Vector3d acceleration;
  ComputeDesiredAcceleration(&acceleration);

  Eigen::Vector3d angular_acceleration;
  ComputeDesiredAngularAcc(acceleration, &angular_acceleration);

  // project thrust to body z axis.
  double thrust = -vehicle_parameters_.mass_ * acceleration.dot(odometry_.orientation.toRotationMatrix().col(2));

  Eigen::Vector4d angular_acceleration_thrust;
  angular_acceleration_thrust.block<3, 1>(0, 0) = angular_acceleration;
  angular_acceleration_thrust(3) = thrust;

  *rotor_velocities = angular_acc_to_rotor_velocities_ * angular_acceleration_thrust;
  *rotor_velocities = rotor_velocities->cwiseMax(Eigen::VectorXd::Zero(rotor_velocities->rows()));
  *rotor_velocities = rotor_velocities->cwiseSqrt();
}

void LeePositionController::SetOdometry(const EigenOdometry& odometry) {
  odometry_ = odometry;
}

void LeePositionController::SetCommandTrajectory(
    const mav_msgs::EigenCommandTrajectory& command_trajectory) {
  command_trajectory_ = command_trajectory;
}

void LeePositionController::ComputeDesiredAcceleration(Eigen::Vector3d* acceleration) const {
  assert(acceleration);

  Eigen::Vector3d position_error;
  position_error = odometry_.position - command_trajectory_.position;

  // Transform velocity to world frame.
  const Eigen::Matrix3d R_W_I = odometry_.orientation.toRotationMatrix();
  Eigen::Vector3d velocity_W =  R_W_I * odometry_.velocity;
  Eigen::Vector3d velocity_error;
  velocity_error = velocity_W - command_trajectory_.velocity;

  Eigen::Vector3d e_3(0, 0, 1);

  *acceleration = (position_error.cwiseProduct(controller_parameters_.position_gain_)
      + velocity_error.cwiseProduct(controller_parameters_.velocity_gain_)) / vehicle_parameters_.mass_
      - physics_parameters_.gravity_ * e_3 - command_trajectory_.acceleration;
}

// Implementation from the T. Lee et al. paper
// Control of complex maneuvers for a quadrotor UAV using geometric methods on SE(3)
void LeePositionController::ComputeDesiredAngularAcc(const Eigen::Vector3d& acceleration,
                                                     Eigen::Vector3d* angular_acceleration) const {
  assert(angular_acceleration);

  Eigen::Matrix3d R = odometry_.orientation.toRotationMatrix();

  // get desired rotation matrix
  Eigen::Vector3d b1_des;
  b1_des << cos(command_trajectory_.yaw), sin(command_trajectory_.yaw), 0;

  Eigen::Vector3d b3_des;
  b3_des = -acceleration / acceleration.norm();

  Eigen::Vector3d b2_des;
  b2_des = b3_des.cross(b1_des);
  b2_des.normalize();

  Eigen::Matrix3d R_des;
  R_des.col(0) = b2_des.cross(b3_des);
  R_des.col(1) = b2_des;
  R_des.col(2) = b3_des;

  // angle error according to lee et al.
  Eigen::Matrix3d angle_error_matrix = 0.5 * (R_des.transpose() * R - R.transpose() * R_des);
  Eigen::Vector3d angle_error;
  angle_error << angle_error_matrix(2, 1),  // inverse skew operator
  angle_error_matrix(0, 2), angle_error_matrix(1, 0);

  // TODO(burrimi) include angular rate references at some point.
  Eigen::Vector3d angular_rate_des(Eigen::Vector3d::Zero());
  angular_rate_des[2] = command_trajectory_.yaw_rate;

  Eigen::Vector3d angular_rate_error = odometry_.angular_velocity - R_des.transpose() * R * angular_rate_des;

  *angular_acceleration = -1 * angle_error.cwiseProduct(normalized_attitude_gain_)
                           - angular_rate_error.cwiseProduct(normalized_angular_rate_gain_)
                           + odometry_.angular_velocity.cross(odometry_.angular_velocity); // we don't need the inertia matrix here
}
}
