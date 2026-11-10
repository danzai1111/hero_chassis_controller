# 10–15 分钟汇报录屏脚本

本稿使用已经实际验证的结果。团队 PPT 模板收到后，可把每一节拆成一页或两页。

## 1. 开场（约 40 秒）

介绍姓名、年级和个人仓库。说明任务：在 ROS Noetic 和 Gazebo 中实现
Hero 四轮麦克纳姆底盘控制器。

核心链路一句话：

```text
/cmd_vel → 麦轮逆解 → 四轮目标角速度
→ 四个 PID 输出力矩 → 四轮关节
→ 实测轮速正解 → /odom 与 odom→base_link TF
```

## 2. 环境与两个参考仓库（约 1 分钟）

展示：

```text
Ubuntu 26.04.1 宿主机
ROS Noetic 1.17.4（Docker）
Gazebo Classic 11.15.1
CLion 2026.2.4.1
```

展示两个参考仓库均为 `master`：

```text
rm_description_for_task  b88724e
simple_chassis_controller a7eeafb
```

说明镜像原本没有 GDB，因此创建了只增加 GDB 和 gdbserver 的派生镜像；
原 `ros_noetic` 容器没有删除或重建。

## 3. 参考控制器与调试（约 2 分钟）

说明参考控制器的职责：

- `init()` 从 `EffortJointInterface` 获取四个轮关节。
- `update()` 每两秒切换一组固定力矩。
- 它没有 `/cmd_vel`、逆解和 PID，是调试样例，不是最终控制器。

展示 `reference_controller_build.png`，说明：

```text
Built target simple_chassis_controller
```

展示 `reference_controller_debug.png`，说明：

- CLion/GDB 附加到 `gzserver`。
- 命中 `update()` 第 37 行。
- `state_=3`、`tau=0.2`，四个 `JointHandle` 有效。

## 4. 自己的控制器构建（约 1 分钟）

说明仓库从 `rm_template` 创建，保留 Git 历史，然后导入自己的源码。

展示成功构建：

```text
Building CXX object .../hero_chassis_controller.cpp.o
Linking CXX shared library devel/lib/libhero_chassis_controller.so
Built target hero_chassis_controller
```

说明控制器通过 pluginlib 加载；YAML 指定插件类型，launch 请求
`controller_manager` 创建控制器。

## 5. 几何核实（约 1 分钟）

展示 Xacro 来源：

```text
wheel_radius = 0.07625 m
wheel_track  = 0.4 m
wheel_base   = 0.4 m
wheel_x_offset = 0.2 m
wheel_y_offset = 0.2 m
```

说明旋转力臂：

```text
k = wheel_x_offset + wheel_y_offset = 0.4 m
```

## 6. 逆运动学与基础运动（约 1 分 30 秒）

依次展示并讲解：

| 指令 | 目标轮速 FL/FR/BL/BR |
| --- | --- |
| `linear.x=0.1` | `+ + + +` |
| `linear.y=0.1` | `− + + −` |
| `angular.z=0.3` | `− + − +` |

实测重点：

- 前进时 Gazebo 车体 x 增加。
- 左移时 y 从约 `0` 增至 `0.673 m`。
- 逆时针时车体转动约 90°。

## 7. 四轮 PID（约 2 分钟）

说明误差和 PID：

```text
error = target_wheel_speed - actual_wheel_speed
effort = PID(error, period)
```

展示 `pid_tuning_rqt.png`：

- 目标前左轮速约 `1.311 rad/s`。
- 实测轮速约 `1.28–1.39 rad/s`。
- 四轮最终参数 `P=1.2, I=0, D=0, i_clamp=2.0`。

说明 P 从 `0.8` 调到 `1.2` 后跟随更快，但没有持续振荡。

## 8. 正运动学、里程计、TF（约 2 分钟）

展示 `/odom`：

```bash
rostopic echo -n 1 /odom/pose/pose
```

展示 TF：

```bash
rosrun tf tf_echo odom base_link
```

实际一致性：

```text
/odom: x≈0.683, y≈-0.001, quaternion z≈0.685, w≈0.729
TF:    x=0.683, y=-0.001, quaternion z=0.685, w=0.729
```

说明 `/odom` 和 `odom -> base_link` 来自同一积分位姿。

## 9. global 模式（约 1 分 30 秒）

先转向约 88°，再发送世界坐标中的：

```bash
rostopic pub -r 10 /cmd_vel geometry_msgs/Twist \
  '{linear: {x: 0.1}}'
```

实测 Gazebo 世界速度：

```text
linear.x≈0.090 m/s
linear.y≈-0.0005 m/s
```

说明 global 模式使用 TF 的 `transformVector()` 将世界速度旋转到
`base_link`，所以车头不再朝世界 x 时仍沿世界 x 移动。

## 10. 特色功能（约 1 分钟）

- 停止发布 `/cmd_vel`，目标轮速在 `command_timeout=0.5 s` 后归零。
- 加速度限制为 `12 rad/s²`，目标轮速逐步变化。
- 键盘节点命令：

```bash
rosrun teleop_twist_keyboard teleop_twist_keyboard.py cmd_vel:=/cmd_vel
```

如果现场键盘演示未成功，只说明启动方式，不宣称已经验证。

## 11. Git 与总结（约 1 分钟）

展示：

```bash
git log --oneline --graph --decorate --all
```

说明有两个个人练习分支，修改同一行后产生真实冲突，最终保留 body 和
global 两种演示说明。明确这是个人练习，不是多人协作。

最后总结已经验证的功能，并明确尚未完成的项目，例如团队 PPT 模板、
最终 MP4 和远程仓库推送。
