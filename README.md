# hero_chassis_controller

麦克纳姆轮英雄底盘的 ROS1 `ros_control` 控制器插件，面向 ROS Noetic 和 Gazebo。它订阅 `/cmd_vel`，为四个轮子分别计算期望角速度并由 `control_toolbox::Pid` 输出力矩；同时根据实测轮速发布 `/odom` 和 `odom -> base_link` TF。

## 控制器做什么

控制器按以下顺序工作：

1. 收到 `/cmd_vel` 的 `geometry_msgs/Twist`。
2. 在 `body` 模式下直接按 `base_link` 理解指令；在 `global` 模式下通过 TF 将 `world_frame_id` 中的平移速度向量变换到 `base_link`。
3. 用麦轮逆运动学得到四个轮子的期望角速度。
4. 分别比较每个期望轮速和关节反馈轮速，用四个独立 PID 计算力矩。
5. 用四个实测轮速做正运动学，积分位姿，并发布 `/odom` 与 `odom -> base_link`。

设计推导见 [`doc/design.md`](doc/design.md)，实际验证结果见 [`doc/validation.md`](doc/validation.md)，Git 分支和合并记录见 [`doc/git_notes.md`](doc/git_notes.md)。

## 仿真前要核对模型参数

`config/controllers.yaml` 中的 `wheel_radius`、`wheel_x_offset`、`wheel_y_offset` 是以米为单位的几何参数。当前值已从 `rm_description_for_task` 的 Hero Xacro 核实：`wheel_track=0.4 m`、`wheel_base=0.4 m`、`wheel_radius=0.07625 m`。偏置是轮心坐标的绝对值，不是左右轮或前后轮之间的全间距。控制器用 `wheel_x_offset + wheel_y_offset = 0.4 m` 作为麦轮转动项的力臂。

配置里的 PID 已在 Gazebo 中使用 `rqt_plot` 和 `rqt_reconfigure` 调参。当前四轮使用 `p=1.2`、`i=0`、`d=0`、`i_clamp=2.0`，目标与实测轮速曲线见 `doc/evidence/pid_tuning_rqt.png`。

## 编译与运行

构建依赖包括 `control_msgs`、`control_toolbox`、`controller_interface`、`geometry_msgs`、`hardware_interface`、`nav_msgs`、`pluginlib`、`realtime_tools`、`roscpp`、`std_msgs` 和 `tf`。仿真运行还需要 `rm_description_for_task`、`gazebo_ros_control`、`controller_manager` 和 `joint_state_controller`；键盘遥控使用 `teleop_twist_keyboard`。PID 调参演示需要安装 `rqt_reconfigure` 和 `rqt_plot`。

推荐使用 CLion 的 Docker 工具链打开包内的 `CMakeLists.txt`，选择 `Debug` 配置完成构建。运行 `.launch` 后可将调试器连接到 `gzserver`。

先在 CLion 中成功编译，然后运行：

```bash
roslaunch hero_chassis_controller run_simulation_and_controller.launch
```

全局速度模式：

```bash
roslaunch hero_chassis_controller run_simulation_and_controller.launch command_mode:=global
```

键盘控制要在能接收键盘输入的交互式终端中运行。推荐另开一个终端直接运行（并保持该终端窗口有焦点）：

```bash
rosrun teleop_twist_keyboard teleop_twist_keyboard.py cmd_vel:=/cmd_vel
```

上游也以 `rosrun` 作为键盘节点的运行方式。`teleop_keyboard.launch` 是可选启动封装；如果它启动后按键没有响应，改用上面的交互式终端命令。参见 [`teleop_twist_keyboard` 上游说明](https://github.com/ros-teleop/teleop_twist_keyboard#launch)。

仿真默认不暂停；若启动时显式传入 `paused:=true`，需要先在 Gazebo 中继续仿真。

## 配置

`config/controllers.yaml` 定义以下配置：

| 参数 | 说明 |
| --- | --- |
| `joints` | 按前左、前右、后左、后右排列的四个关节名 |
| `wheel_radius` | 轮半径，单位 m |
| `wheel_x_offset`, `wheel_y_offset` | 轮心到 `base_link` 原点的纵向、横向距离，单位 m |
| `command_mode` | `body` 或 `global` |
| `base_frame_id`, `odom_frame_id`, `world_frame_id` | 底盘、里程计和全局指令坐标系 |
| `max_wheel_speed` | 轮速上限，单位 rad/s；超限时按同一比例缩放四轮指令 |
| `max_wheel_acceleration` | 轮速变化上限，单位 rad/s²；设为 0 时关闭 |
| `max_effort` | PID 输出力矩限幅，单位 N·m |
| `publish_rate` | `/odom` 和 TF 发布频率，单位 Hz |
| `pid/<joint>/p,i,d,i_clamp` | 各轮独立 PID 参数；支持动态调参 |

## 观测与调参

- `/cmd_vel` (`geometry_msgs/Twist`)：底盘速度指令。
- `/odom` (`nav_msgs/Odometry`)：由实测轮速计算的速度和里程计位姿。
- `/controller/hero_chassis_controller/wheel_state` (`std_msgs/Float64MultiArray`)：按 `[FL目标,FL实测,FR目标,FR实测,BL目标,BL实测,BR目标,BR实测]` 保存四轮角速度；例如 `data[0]` 是前左轮目标值，`data[1]` 是前左轮实测值。
- `/controller/hero_chassis_controller/pid/<joint>/state` (`control_msgs/PidState`)：每个 PID 启用 `publish_state` 后发布内部 P/I/D 状态。
- `rqt_reconfigure`：在四个 PID 各自的动态参数服务中调节 `p/i/d` 和积分限幅。
- `rqt_plot`：绘制 `/controller/hero_chassis_controller/wheel_state/data[0]` 与 `data[1]` 等期望/实测轮速曲线。
- `rosrun tf tf_echo odom base_link`：查看里程计到车体的坐标变换。

## 演示建议

1. 用 `linear.x > 0` 演示前进，用 `linear.y > 0` 演示左移，用 `angular.z > 0` 演示逆时针旋转。
2. 展示四轮目标轮速与实测轮速曲线，说明 PID 如何减小误差，并现场调一组 PID 参数。
3. 展示 `/odom` 与 `odom -> base_link` TF。
4. 切换 `command_mode:=global`，让机器人先转向，再给世界坐标系中的 x/y 指令，说明 TF 如何把指令转换到底盘坐标系。
5. 停止发送 `/cmd_vel`，确认超时后轮速目标归零；演示键盘控制和加速度限制。

本控制器已在 ROS Noetic、Gazebo Classic 11.15.1 和 CLion Docker 工具链中成功编译和运行；前进、左移、逆时针、PID 轮速跟随、`/odom`、TF 与 global 模式均已实际验证。键盘遥控启动文件已包含在 `launch/` 中。
