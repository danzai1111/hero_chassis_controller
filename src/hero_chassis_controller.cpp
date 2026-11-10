#include <algorithm>
#include <cmath>
#include <cstddef>

#include <pluginlib/class_list_macros.hpp>
#include <tf/transform_datatypes.h>

#include "hero_chassis_controller/hero_chassis_controller.h"

namespace hero_chassis_controller {

namespace {

constexpr std::size_t kFrontLeft = 0;
constexpr std::size_t kFrontRight = 1;
constexpr std::size_t kBackLeft = 2;
constexpr std::size_t kBackRight = 3;

}  // namespace

bool HeroChassisController::init(
    hardware_interface::EffortJointInterface* effort_interface,
    ros::NodeHandle& root_nh,
    ros::NodeHandle& controller_nh) {
  if (effort_interface == nullptr) {
    ROS_ERROR("The effort joint interface is null.");
    return false;
  }

  if (!controller_nh.getParam("joints", wheel_joint_names_) ||
      wheel_joint_names_.size() != kWheelCount) {
    ROS_ERROR("Parameter 'joints' must list exactly four wheel joints in "
              "FL, FR, BL, BR order.");
    return false;
  }

  if (!controller_nh.getParam("wheel_radius", wheel_radius_) ||
      !std::isfinite(wheel_radius_) || wheel_radius_ <= 0.0 ||
      !controller_nh.getParam("wheel_x_offset", wheel_x_offset_) ||
      !std::isfinite(wheel_x_offset_) || wheel_x_offset_ <= 0.0 ||
      !controller_nh.getParam("wheel_y_offset", wheel_y_offset_) ||
      !std::isfinite(wheel_y_offset_) || wheel_y_offset_ <= 0.0) {
    ROS_ERROR("wheel_radius, wheel_x_offset, and wheel_y_offset must be "
              "positive metres.");
    return false;
  }

  controller_nh.param("base_frame_id", base_frame_id_, base_frame_id_);
  controller_nh.param("odom_frame_id", odom_frame_id_, odom_frame_id_);
  controller_nh.param("world_frame_id", world_frame_id_, world_frame_id_);
  controller_nh.param("command_mode", command_mode_, command_mode_);
  controller_nh.param("command_timeout", command_timeout_, command_timeout_);
  controller_nh.param("max_wheel_speed", max_wheel_speed_, 15.0);
  controller_nh.param("max_wheel_acceleration", max_wheel_acceleration_, 10.0);
  controller_nh.param("max_effort", max_effort_, 2.0);
  double publish_rate = 50.0;
  controller_nh.param("publish_rate", publish_rate, 50.0);
  controller_nh.param("publish_tf", publish_tf_, publish_tf_);

  if (command_mode_ != "body" && command_mode_ != "global") {
    ROS_ERROR("command_mode must be either 'body' or 'global'.");
    return false;
  }
  if (!std::isfinite(command_timeout_) || command_timeout_ <= 0.0 ||
      !std::isfinite(max_wheel_speed_) || max_wheel_speed_ <= 0.0 ||
      !std::isfinite(max_effort_) || max_effort_ <= 0.0 ||
      !std::isfinite(publish_rate) || publish_rate <= 0.0 ||
      !std::isfinite(max_wheel_acceleration_) ||
      max_wheel_acceleration_ < 0.0) {
    ROS_ERROR("Timeout, wheel speed, effort, and publish rate must be "
              "positive; wheel acceleration must be non-negative.");
    return false;
  }
  odometry_publish_period_ = 1.0 / publish_rate;

  wheel_joints_.clear();
  wheel_joints_.reserve(kWheelCount);
  for (std::size_t index = 0; index < kWheelCount; ++index) {
    try {
      wheel_joints_.push_back(
          effort_interface->getHandle(wheel_joint_names_[index]));
    } catch (const hardware_interface::HardwareInterfaceException& exception) {
      ROS_ERROR_STREAM("Could not get wheel joint '"
                       << wheel_joint_names_[index] << "': "
                       << exception.what());
      return false;
    }

    ros::NodeHandle pid_nh(controller_nh, "pid/" + wheel_joint_names_[index]);
    if (!wheel_pids_[index].init(pid_nh)) {
      ROS_ERROR_STREAM("Could not initialize PID for "
                       << wheel_joint_names_[index]
                       << ". Check its p/i/d parameters.");
      return false;
    }
  }

  VelocityCommand initial_command;
  initial_command.twist = geometry_msgs::Twist();
  initial_command.valid = false;
  command_buffer_.writeFromNonRT(initial_command);

  command_subscriber_ = root_nh.subscribe(
      "/cmd_vel", 1, &HeroChassisController::commandCallback, this);

  odom_publisher_.reset(
      new realtime_tools::RealtimePublisher<nav_msgs::Odometry>(
          root_nh, "/odom", 20));
  odom_publisher_->msg_.header.frame_id = odom_frame_id_;
  odom_publisher_->msg_.child_frame_id = base_frame_id_;

  wheel_state_publisher_.reset(
      new realtime_tools::RealtimePublisher<std_msgs::Float64MultiArray>(
          controller_nh, "wheel_state", 10));
  wheel_state_publisher_->msg_.data.resize(kWheelCount * 2, 0.0);
  wheel_state_publisher_->msg_.layout.dim.resize(1);
  wheel_state_publisher_->msg_.layout.dim[0].label =
      "FL_target,FL_actual,FR_target,FR_actual,"
      "BL_target,BL_actual,BR_target,BR_actual";
  wheel_state_publisher_->msg_.layout.dim[0].size = kWheelCount * 2;
  wheel_state_publisher_->msg_.layout.dim[0].stride = kWheelCount * 2;
  wheel_state_publisher_->msg_.layout.data_offset = 0;

  if (publish_tf_) {
    tf_publisher_.reset(
        new realtime_tools::RealtimePublisher<tf::tfMessage>(
            root_nh, "/tf", 20));
    tf_publisher_->msg_.transforms.resize(1);
    tf_publisher_->msg_.transforms[0].header.frame_id = odom_frame_id_;
    tf_publisher_->msg_.transforms[0].child_frame_id = base_frame_id_;
  }

  ROS_INFO_STREAM("Hero chassis controller initialized with command_mode='"
                  << command_mode_ << "', wheel radius " << wheel_radius_
                  << " m.");
  return true;
}

void HeroChassisController::starting(const ros::Time& time) {
  last_odom_publish_time_ = time.toSec();
  VelocityCommand initial_command;
  initial_command.twist = geometry_msgs::Twist();
  initial_command.stamp = time;
  initial_command.valid = false;
  command_buffer_.writeFromNonRT(initial_command);
  limited_wheel_speeds_.fill(0.0);
  body_linear_x_ = 0.0;
  body_linear_y_ = 0.0;
  body_angular_z_ = 0.0;
  for (std::size_t index = 0; index < kWheelCount; ++index) {
    wheel_pids_[index].reset();
    wheel_joints_[index].setCommand(0.0);
  }
}

void HeroChassisController::update(
    const ros::Time& time, const ros::Duration& period) {
  const double dt = period.toSec();
  if (dt <= 0.0 || !std::isfinite(dt)) {
    for (auto& joint : wheel_joints_) {
      joint.setCommand(0.0);
    }
    return;
  }

  VelocityCommand command = *(command_buffer_.readFromRT());
  if (!command.valid || (time - command.stamp).toSec() > command_timeout_) {
    command.twist = geometry_msgs::Twist();
  }

  const std::array<double, kWheelCount> requested_wheel_speeds =
      inverseKinematics(command.twist);
  double largest_speed = 0.0;
  for (const double speed : requested_wheel_speeds) {
    largest_speed = std::max(largest_speed, std::abs(speed));
  }

  const double scale = largest_speed > max_wheel_speed_
                           ? max_wheel_speed_ / largest_speed
                           : 1.0;
  for (std::size_t index = 0; index < kWheelCount; ++index) {
    const double target = requested_wheel_speeds[index] * scale;
    if (max_wheel_acceleration_ > 0.0) {
      const double max_delta = max_wheel_acceleration_ * dt;
      const double delta = target - limited_wheel_speeds_[index];
      limited_wheel_speeds_[index] +=
          std::max(-max_delta, std::min(delta, max_delta));
    } else {
      limited_wheel_speeds_[index] = target;
    }

    const double actual_speed = wheel_joints_[index].getVelocity();
    double effort = wheel_pids_[index].computeCommand(
        limited_wheel_speeds_[index] - actual_speed, period);
    effort = std::max(-max_effort_, std::min(effort, max_effort_));
    wheel_joints_[index].setCommand(effort);
  }

  const double front_left = wheel_joints_[kFrontLeft].getVelocity();
  const double front_right = wheel_joints_[kFrontRight].getVelocity();
  const double back_left = wheel_joints_[kBackLeft].getVelocity();
  const double back_right = wheel_joints_[kBackRight].getVelocity();
  const double lever = wheel_x_offset_ + wheel_y_offset_;

  body_linear_x_ = wheel_radius_ *
                   (front_left + front_right + back_left + back_right) / 4.0;
  body_linear_y_ = wheel_radius_ *
                   (-front_left + front_right + back_left - back_right) / 4.0;
  body_angular_z_ = wheel_radius_ *
                    (-front_left + front_right - back_left + back_right) /
                    (4.0 * lever);

  const double middle_yaw = yaw_ + 0.5 * body_angular_z_ * dt;
  x_ += (std::cos(middle_yaw) * body_linear_x_ -
         std::sin(middle_yaw) * body_linear_y_) * dt;
  y_ += (std::sin(middle_yaw) * body_linear_x_ +
         std::cos(middle_yaw) * body_linear_y_) * dt;
  yaw_ = normalizeAngle(yaw_ + body_angular_z_ * dt);
  if (wheel_state_publisher_ && wheel_state_publisher_->trylock()) {
    for (std::size_t index = 0; index < kWheelCount; ++index) {
      wheel_state_publisher_->msg_.data[index * 2] =
          limited_wheel_speeds_[index];
      wheel_state_publisher_->msg_.data[index * 2 + 1] =
          wheel_joints_[index].getVelocity();
    }
    wheel_state_publisher_->unlockAndPublish();
  }

  if (time.toSec() - last_odom_publish_time_ >= odometry_publish_period_) {
    publishOdometry(time, body_linear_x_, body_linear_y_, body_angular_z_);
    last_odom_publish_time_ = time.toSec();
  }
}

void HeroChassisController::stopping(const ros::Time& /*time*/) {
  for (std::size_t index = 0; index < kWheelCount; ++index) {
    wheel_pids_[index].reset();
    wheel_joints_[index].setCommand(0.0);
  }
  limited_wheel_speeds_.fill(0.0);
}

void HeroChassisController::commandCallback(
    const geometry_msgs::TwistConstPtr& message) {
  VelocityCommand command;
  command.twist = *message;
  command.stamp = ros::Time::now();

  if (!std::isfinite(message->linear.x) || !std::isfinite(message->linear.y) ||
      !std::isfinite(message->angular.z)) {
    ROS_WARN_THROTTLE(1.0,
                      "Ignoring /cmd_vel containing a non-finite planar "
                      "velocity.");
    command.twist = geometry_msgs::Twist();
    command.valid = false;
    command_buffer_.writeFromNonRT(command);
    return;
  }
  command.valid = true;

  if (command_mode_ == "global") {
    geometry_msgs::Vector3Stamped world_vector;
    geometry_msgs::Vector3Stamped body_vector;
    world_vector.header.frame_id = world_frame_id_;
    world_vector.header.stamp = ros::Time(0);
    world_vector.vector.x = message->linear.x;
    world_vector.vector.y = message->linear.y;
    world_vector.vector.z = 0.0;
    try {
      transform_listener_.transformVector(
          base_frame_id_, world_vector, body_vector);
      command.twist.linear.x = body_vector.vector.x;
      command.twist.linear.y = body_vector.vector.y;
      command.twist.linear.z = 0.0;
    } catch (const tf::TransformException& exception) {
      ROS_WARN_THROTTLE(1.0, "Cannot transform global /cmd_vel into %s: %s",
                        base_frame_id_.c_str(), exception.what());
      command.twist = geometry_msgs::Twist();
    }
  }

  command_buffer_.writeFromNonRT(command);
}

std::array<double, HeroChassisController::kWheelCount>
HeroChassisController::inverseKinematics(
    const geometry_msgs::Twist& body_command) const {
  const double lever = wheel_x_offset_ + wheel_y_offset_;
  const double vx = body_command.linear.x;
  const double vy = body_command.linear.y;
  const double wz = body_command.angular.z;
  const double inverse_radius = 1.0 / wheel_radius_;

  // Positive x is forward, positive y is left, and positive z rotation is CCW.
  // Wheel order is FL, FR, BL, BR, matching the source model's joint names.
  return {{(vx - vy - lever * wz) * inverse_radius,
           (vx + vy + lever * wz) * inverse_radius,
           (vx + vy - lever * wz) * inverse_radius,
           (vx - vy + lever * wz) * inverse_radius}};
}

void HeroChassisController::publishOdometry(const ros::Time& time,
                                            double linear_x,
                                            double linear_y,
                                            double angular_z) {
  const geometry_msgs::Quaternion orientation =
      tf::createQuaternionMsgFromYaw(yaw_);

  if (odom_publisher_ && odom_publisher_->trylock()) {
    nav_msgs::Odometry& odometry = odom_publisher_->msg_;
    odometry.header.stamp = time;
    odometry.header.frame_id = odom_frame_id_;
    odometry.child_frame_id = base_frame_id_;
    odometry.pose.pose.position.x = x_;
    odometry.pose.pose.position.y = y_;
    odometry.pose.pose.position.z = 0.0;
    odometry.pose.pose.orientation = orientation;
    odometry.twist.twist.linear.x = linear_x;
    odometry.twist.twist.linear.y = linear_y;
    odometry.twist.twist.linear.z = 0.0;
    odometry.twist.twist.angular.x = 0.0;
    odometry.twist.twist.angular.y = 0.0;
    odometry.twist.twist.angular.z = angular_z;
    odom_publisher_->unlockAndPublish();
  }

  if (publish_tf_ && tf_publisher_ && tf_publisher_->trylock()) {
    geometry_msgs::TransformStamped& transform =
        tf_publisher_->msg_.transforms[0];
    transform.header.stamp = time;
    transform.header.frame_id = odom_frame_id_;
    transform.child_frame_id = base_frame_id_;
    transform.transform.translation.x = x_;
    transform.transform.translation.y = y_;
    transform.transform.translation.z = 0.0;
    transform.transform.rotation = orientation;
    tf_publisher_->unlockAndPublish();
  }
}

double HeroChassisController::normalizeAngle(double angle) {
  return std::atan2(std::sin(angle), std::cos(angle));
}

}  // namespace hero_chassis_controller

PLUGINLIB_EXPORT_CLASS(hero_chassis_controller::HeroChassisController,
                       controller_interface::ControllerBase)
