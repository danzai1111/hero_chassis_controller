# hero_chassis_controller

麦克纳姆轮英雄底盘的 ROS1 `ros_control` 控制器插件，面向 ROS Noetic 和 Gazebo。它订阅 `/cmd_vel`，为四个轮子分别计算期望角速度并由 `control_toolbox::Pid` 输出力矩；同时根据实测轮速发布 `/odom` 和 `odom -> base_link` TF。

## 控制器做什么

控制器按以下顺序工作：

1. 收到 `/cmd_vel` 的 `geometry_msgs/Twist`。
2. 在 `body` 模式下直接按 `base_link` 理解指令；在 `global` 模式下通过 TF 将 `world_frame_id` 中的平移速度向量变换到 `base_link`。
3. 用麦轮逆运动学得到四个轮子的期望角速度。
4. 分别比较每个期望轮速和关节反馈轮速，用四个独立 PID 计算力矩。
5. 用四个实测轮速做正运动学，积分位姿，并发布 `/odom` 与 `odom -> base_link`。

代码采用 ROS C++ 命名空间和 Google 风格的大括号、缩进及命名约定。公式推导和逐项学习笔记见 [`doc/design_and_learning.md`](doc/design_and_learning.md)，答辩口述练习见 [`doc/defense_qa.md`](doc/defense_qa.md)。

从零运行的 Linux/CLion 步骤见 [`doc/start_here_linux.md`](doc/start_here_linux.md)；评分证据清单见 [`doc/assessment_checklist.md`](doc/assessment_checklist.md)；仿真数据记录模板见 [`doc/simulation_log_template.md`](doc/simulation_log_template.md)；11 月 12 日截止的冲刺计划见 [`doc/deadline_plan.md`](doc/deadline_plan.md)；10–15 分钟视频提纲见 [`doc/presentation_outline.md`](doc/presentation_outline.md)。

按 `rm_template` 建立个人仓库、规划真实提交和记录冲突练习的方法见 [`doc/version_control_guide.md`](doc/version_control_guide.md)。

必做参考控制器的类成员、`state_`/`last_change_`、固定力矩动作和 pluginlib 加载链逐段学习路线见 [`doc/reference_controller_walkthrough.md`](doc/reference_controller_walkthrough.md)。

四轮 PID 的 rqt_reconfigure、rqt_plot 操作和调参记录步骤见 [`doc/pid_tuning_guide.md`](doc/pid_tuning_guide.md)。

从指定 Hero Xacro/展开 URDF 核对轮半径、轮心偏置和轴方向的方法见 [`doc/geometry_verification_guide.md`](doc/geometry_verification_guide.md)。配置中的候选几何值未确认前不要用作最终调参或汇报结果。

汇报视频的逐页口述稿草稿见 [`doc/presentation_script.md`](doc/presentation_script.md)。方括号里的内容需要用真实调试、调参和仿真证据替换；收到团队统一 PPT 模板后再套版。

## 仿真前要核对模型参数

`config/controllers.yaml` 中的 `wheel_radius`、`wheel_x_offset`、`wheel_y_offset` 是以米为单位的几何参数。当前数值只是第一版起点；正式演示前请从 `rm_description_for_task` 的 Hero Xacro 中核实轮半径及轮心相对 `base_link` 的 x/y 坐标。偏置是轮心坐标的绝对值，不是左右轮或前后轮之间的全间距。控制器用 `wheel_x_offset + wheel_y_offset` 作为麦轮转动项的力臂。

提交前请将 `package.xml` 的维护者姓名和邮箱替换为你自己的信息。

压缩包包含 ROS 包源码，不含 Git 历史。按考核要求，用 `rm_template` 创建你自己的 `hero_chassis_controller` 仓库，再逐项提交源码和运行记录，保留真实的提交与合并记录。

配置里的 PID 值也是启动初值，不能代替仿真调参。请在 Gazebo 中观察期望/实测轮速，用 `rqt_plot` 和 `rqt_reconfigure` 调整四个轮子的参数，并保存最终参数。

## 编译与运行

构建依赖包括 `control_msgs`、`control_toolbox`、`controller_interface`、`geometry_msgs`、`hardware_interface`、`nav_msgs`、`pluginlib`、`realtime_tools`、`roscpp`、`std_msgs` 和 `tf`。仿真运行还需要 `rm_description_for_task`（ROS 包名 `rm_description`）、`gazebo_ros_control`、`controller_manager` 和 `joint_state_controller`；键盘遥控使用 `teleop_twist_keyboard`。PID 调参演示需要安装 `rqt_reconfigure` 和 `rqt_plot`。

按考核要求用 CLion 编译和调试。在使用 catkin-tools 的工作区中，CLion 应分别打开包内的 `CMakeLists.txt`；从已加载 ROS Noetic 环境的 shell 启动 CLion，并使用 `Debug` 配置。运行 `.launch` 后在 CLion 中附加到 `gzserver`。若需要 catkin 命令行工具，使用 `catkin build`，不要使用 `catkin_make`。详细操作见 [`doc/start_here_linux.md`](doc/start_here_linux.md)。

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

演示结果必须以你实际运行的 Gazebo 画面和 ROS 输出为准；本仓库当前尚未在你的 Ubuntu/CLion 环境中编译或运行。
