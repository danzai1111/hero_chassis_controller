#ifndef HERO_CHASSIS_CONTROLLER_HERO_CHASSIS_CONTROLLER_H_
#define HERO_CHASSIS_CONTROLLER_HERO_CHASSIS_CONTROLLER_H_

#include <control_toolbox/pid.h>
#include <controller_interface/controller.h>
#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <hardware_interface/joint_command_interface.h>
#include <nav_msgs/Odometry.h>
#include <realtime_tools/realtime_buffer.h>
#include <realtime_tools/realtime_publisher.h>
#include <ros/ros.h>
#include <std_msgs/Float64MultiArray.h>
#include <tf/tfMessage.h>
#include <tf/transform_listener.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace hero_chassis_controller {

/**
 * Four-wheel mecanum controller for the Hero chassis.
 *
 * The effort interface is intentional: each wheel PID compares its target
 * angular velocity with the joint's measured angular velocity and sends the
 * resulting torque to the simulated (or real) joint.
 */
class HeroChassisController : public controller_interface::Controller<
                                  hardware_interface::EffortJointInterface> {
 public:
  HeroChassisController() = default;
  ~HeroChassisController() override = default;

  bool init(hardware_interface::EffortJointInterface* effort_interface,
            ros::NodeHandle& root_nh, ros::NodeHandle& controller_nh) override;
  void starting(const ros::Time& time) override;
  void update(const ros::Time& time, const ros::Duration& period) override;
  void stopping(const ros::Time& time) override;

 private:
  static constexpr std::size_t kWheelCount = 4;

  struct VelocityCommand {
    geometry_msgs::Twist twist;
    ros::Time stamp;
    // Timestamp zero is valid when simulated time starts at zero.
    bool valid{false};
  };

  void commandCallback(const geometry_msgs::TwistConstPtr& message);
  std::array<double, kWheelCount> inverseKinematics(
      const geometry_msgs::Twist& body_command) const;
  void publishOdometry(const ros::Time& time, double linear_x, double linear_y,
                       double angular_z);
  static double normalizeAngle(double angle);

  std::vector<hardware_interface::JointHandle> wheel_joints_;
  std::vector<std::string> wheel_joint_names_;
  std::array<control_toolbox::Pid, kWheelCount> wheel_pids_;

  realtime_tools::RealtimeBuffer<VelocityCommand> command_buffer_;
  std::array<double, kWheelCount> limited_wheel_speeds_{};
  double wheel_radius_{0.0};
  double wheel_x_offset_{0.0};
  double wheel_y_offset_{0.0};
  double max_wheel_speed_{0.0};
  double max_wheel_acceleration_{0.0};
  double max_effort_{0.0};
  double command_timeout_{0.5};
  double odometry_publish_period_{0.02};
  double last_odom_publish_time_{0.0};

  double x_{0.0};
  double y_{0.0};
  double yaw_{0.0};
  double body_linear_x_{0.0};
  double body_linear_y_{0.0};
  double body_angular_z_{0.0};

  std::string base_frame_id_{"base_link"};
  std::string odom_frame_id_{"odom"};
  std::string world_frame_id_{"odom"};
  std::string command_mode_{"body"};
  bool publish_tf_{true};

  ros::Subscriber command_subscriber_;
  std::shared_ptr<realtime_tools::RealtimePublisher<nav_msgs::Odometry>>
      odom_publisher_;
  std::shared_ptr<realtime_tools::RealtimePublisher<tf::tfMessage>>
      tf_publisher_;
  std::shared_ptr<
      realtime_tools::RealtimePublisher<std_msgs::Float64MultiArray>>
      wheel_state_publisher_;
  tf::TransformListener transform_listener_;
};

}  // namespace hero_chassis_controller

#endif  // HERO_CHASSIS_CONTROLLER_HERO_CHASSIS_CONTROLLER_H_
