# Hero 底盘控制器仿真记录

本记录只填写在实际运行中观察到的结果。截图和视频文件后续保存在
`doc/evidence/`，未验证项目单独标记。

## 1. 环境

| 项目 | 实际记录 |
| --- | --- |
| Ubuntu | 26.04.1 LTS（宿主机） |
| Docker 镜像 | `hero-ros-noetic-debug:latest` |
| 基础镜像 | `osrf/ros:noetic-desktop-full` |
| ROS | Noetic，1.17.4 |
| Gazebo | Gazebo Classic 11.15.1 |
| CMake（容器） | 3.16.3 |
| 编译器 | GCC/G++ 9.4 |
| 调试器 | GDB 9.2 |

## 2. 参考工程

| 仓库 | 分支 | commit | 实际结果 |
| --- | --- | --- | --- |
| `rm_description_for_task` | `master` | `b88724e` | Hero 模型成功加载 |
| `simple_chassis_controller` | `master` | `a7eeafb` | CLion 成功编译，控制器成功运行 |

参考控制器的实际调试结果：

- GDB 附加到 `gzserver`。
- 在 `SimpleChassisController::update()` 第 37 行命中真实断点。
- 观察到 `state_ = 3`、`tau = 0.2` 和四个 `JointHandle`。
- `/controller/simple_chassis_controller` 状态为 `running`。
- 四个轮关节由 `EffortJointInterface` 占用。

由于任务镜像没有 `rm_gazebo` 和 `librm_robot_hw_sim.so`，本地采用以下兼容修改：

- `rm_description_for_task/urdf/hero/hero.urdf.xacro` 改用 ROS 自带的
  `gazebo_ros_control/DefaultRobotHWSim`。
- `rm_description_for_task/launch/hero.launch` 使用默认 Gazebo 空世界。
- `simple_chassis_controller/launch/run_simulation_and_controller.launch`
  将 `rm_description` 修正为 `rm_description_for_task`。

## 3. Hero 几何

来源为 `rm_description_for_task/urdf/`：

| 参数 | 来源 | 实际值 |
| --- | --- | ---: |
| 轮半径 | `common/mecanum_wheel.urdf.xacro:3` | `0.07625 m` |
| 轮距基参 `wheel_track` | `hero/chassis.urdf.xacro:6` | `0.4 m` |
| 轴距基参 `wheel_base` | `hero/chassis.urdf.xacro:7` | `0.4 m` |
| 轮心 x 偏置 | `wheel_track / 2` | `0.2 m` |
| 轮心 y 偏置 | `wheel_base / 2` | `0.2 m` |

四个轮关节均连接到 `base_link`，轮轴均为 `0 1 0`。

## 4. PID 调参与实际轮速

最终运行配置：

| 关节 | P | I | D | `i_clamp` |
| --- | ---: | ---: | ---: | ---: |
| `left_front_wheel_joint` | 1.2 | 0.0 | 0.0 | 2.0 |
| `right_front_wheel_joint` | 1.2 | 0.0 | 0.0 | 2.0 |
| `left_back_wheel_joint` | 1.2 | 0.0 | 0.0 | 2.0 |
| `right_back_wheel_joint` | 1.2 | 0.0 | 0.0 | 2.0 |

当前仿真中，命令为 `linear.x = 0.1 m/s`：

- 四轮目标角速度约 `1.3115 rad/s`。
- 开始阶段四轮实测角速度约 `1.21–1.25 rad/s`。
- 后续阶段四轮实测角速度约 `1.28–1.39 rad/s`。
- 未观察到持续振荡。
- 证据：`doc/evidence/pid_tuning_rqt.png`。

动态参数服务均存在：

```text
/controller/hero_chassis_controller/pid/left_front_wheel_joint/set_parameters
/controller/hero_chassis_controller/pid/right_front_wheel_joint/set_parameters
/controller/hero_chassis_controller/pid/left_back_wheel_joint/set_parameters
/controller/hero_chassis_controller/pid/right_back_wheel_joint/set_parameters
```

## 5. 基础运动方向

| 测试 | `/cmd_vel` | 实测目标轮速 FL/FR/BL/BR | 结果 |
| --- | --- | --- | --- |
| 前进 | `linear.x=0.1` | `+ + + +` | Gazebo x 增加，`/odom` vx 约 `0.093 m/s` |
| 左移 | `linear.y=0.1` | `− + + −` | Gazebo y 从约 `0` 增至 `0.673 m` |
| 逆时针 | `angular.z=0.3` | `− + − +` | 车体旋转约 90° |

实测逆时针目标轮速为：

```text
FL=-1.4707, FR=+1.4707, BL=-1.4707, BR=+1.4707 rad/s
```

## 6. 里程计和 TF

一次同一状态下的检查结果：

```text
/odom position: x≈0.00027, y≈-0.00809
tf odom->base_link: x=0.000, y=-0.008
```

global 模式转向约 86° 后的同一状态：

```text
/odom: x≈0.683, y≈-0.001, quaternion z≈0.685, w≈0.729
TF:    x=0.683, y=-0.001, quaternion z=0.685, w=0.729
```

`/odom` 位姿与 `odom -> base_link` TF 一致。

## 7. global 模式

1. 先发送 `angular.z=1.0`，机器人转向约 88°。
2. 再发送世界坐标中的 `linear.x=0.1 m/s`。
3. Gazebo 世界速度实测约 `linear.x=0.090 m/s`、
   `linear.y=-0.0005 m/s`。
4. 车体仍主要沿世界 x 方向移动，而不是沿当前车头方向移动。

该结果证明 global 模式通过 TF 将世界速度转换到了 `base_link`。

## 8. 特色功能

| 功能 | 配置 | 实际观察 |
| --- | --- | --- |
| 命令超时 | `command_timeout=0.5 s` | 停止发布后目标轮速回到 0 |
| 加速度限制 | `max_wheel_acceleration=12 rad/s²` | 目标轮速按控制周期逐步增加 |
| 键盘遥控 | `teleop_twist_keyboard` | 启动文件已提供，交互式按键演示仍待录制 |

## 9. Git

个人仓库本地提交：

```text
6d2e011 feat: import hero chassis controller implementation
8aa5ee6 fix: use verified Hero wheel geometry
cf6f628 fix: tune wheel proportional gains in Gazebo
```

个人 GitHub 远程仓库和最终推送记录待完成。

## 10. 视频交付

- 团队 PPT 模板：待提供
- 10–15 分钟 MP4：待录制
- 文件名“姓名 年级”：待确认
- 视频发送记录：待完成
