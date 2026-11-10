# 考核答辩问答与口述练习

这份材料用于理解代码和练习表达，不是实验结果。方括号中的内容必须在 Ubuntu、CLion 和 Gazebo 中实际完成后填写；没有实测证据时，不要说“已经验证”。

## 先记住这三句话

1. **参考控制器是一个理解插件加载和调试的样例。** 它按固定顺序直接给四个轮子发送力矩测试动作，不是最终需要的速度 PID 控制器。
2. **新控制器把底盘速度变成四个目标轮速。** 每个轮子独立比较目标角速度和实测角速度，再用 PID 计算力矩。
3. **实测轮速还能反推出底盘速度。** 把车体速度积分到 `odom` 坐标系，发布 `/odom` 和同一位姿的 `odom -> base_link` TF。

## 参考工程：`simple_chassis_controller`

### 1. `state_` 和 `last_change_` 是什么？

`state_` 是当前测试动作的编号。参考程序每经过 2 秒，就把它加一并在 6 种动作之间循环；`last_change_` 记录上一次切换动作的 ROS 时间。数组 `cmd_[6][4]` 的每一行是一个动作、每一列是一个轮子的力矩命令。

口述时可以说：“它让我快速观察轮子和底盘对不同力矩组合的反应，但它是固定力矩测试，不会根据目标速度和实际速度的差自动调节。”源码把这段标成 naive testing code，并提醒不要照搬到最终控制器。

### 2. 参考控制器拿到的四个句柄是什么？

`init()` 从 `EffortJointInterface` 依次获取 `left_front_wheel_joint`、`right_front_wheel_joint`、`left_back_wheel_joint`、`right_back_wheel_joint`。这些 `JointHandle` 让控制器读取关节状态并写入力矩命令。数组/命令顺序是前左、前右、后左、后右。

### 3. 参考工程为什么不能算完成了四轮 PID？

它的 `update()` 每 2 秒选择一组预设力矩并直接 `setCommand()`。没有接收底盘 `/cmd_vel`、没有用目标轮速减实际轮速形成误差、也没有调用 `control_toolbox::Pid`。新控制器才实现这条速度闭环。

## 新控制器：核心原理

### 4. 什么是 `EffortJointInterface`？为什么用它做速度控制？

`EffortJointInterface` 的命令量是关节力矩，单位通常是 N·m。本控制器需要控制轮速，因此每周期先读取实际轮速，再由软件 PID 把速度误差变成力矩。接口给的是“执行器输入”，闭环控制器决定如何用这个输入跟踪速度。

```text
目标轮速 - 实际轮速 = 速度误差
PID(速度误差, 周期) = 轮子力矩
```

### 5. `base_link` 坐标约定是什么？

按 ROS 常见平面约定：x 向前、y 向左、z 向上；正 `angular.z` 表示逆时针。公式中四轮顺序固定为 FL、FR、BL、BR。符号方向最终仍要结合指定 Xacro 中的关节轴和 Gazebo 运动验证。

### 6. 逆运动学把什么变成什么？

输入是 `base_link` 中底盘线速度 `(vx, vy)`（m/s）和角速度 `wz`（rad/s）；输出是四个轮子的目标角速度（rad/s）。令半径为 `r`、轮心纵横偏置绝对值为 `a,b`、`k=a+b`：

```text
w_FL = ( vx - vy - k*wz) / r
w_FR = ( vx + vy + k*wz) / r
w_BL = ( vx + vy - k*wz) / r
w_BR = ( vx - vy + k*wz) / r
```

快速检查符号：只前进时四轮同号；只向左时为 `− + + −`；只逆时针时为 `− + − +`。这只是公式的预期符号；真实模型方向必须在仿真中确认。

### 7. 为什么要四个 PID，而不是一个？

每个轮子有自己的目标转速、关节反馈和机械响应。四个 PID 分别处理四个误差，才能补偿电机/关节负载差异，并独立调参。把四轮合成一个误差会丢失每个轮子的跟踪信息。

### 8. `p`、`i`、`d` 和 `i_clamp` 各起什么作用？

- `p` 根据当前误差立即给出修正；过小跟随慢，过大可能振荡。
- `i` 累积长期误差，可帮助消除持续偏差；过大容易积累过多、产生超调。
- `d` 根据误差变化趋势提供阻尼；噪声较大时可能放大抖动。
- `i_clamp` 限制积分项输出的上下界。设成 0 时积分贡献被限制为 0。

`antiwindup: true` 会依据积分限值约束积分状态；它不知道控制器后续对力矩做的 `max_effort` 裁剪，所以仍需观察输出是否长期顶在力矩上限。当前 YAML 的数值是启动初值，不能说是最优参数。实际演示时从较小的 `p` 开始，在 `rqt_plot` 看目标/实测轮速，再逐步调整；每次改动都记下参数和曲线。若要使用积分，再确认 `i_clamp` 是正值且上限合适。`publish_state: true` 会额外发布 PID 内部 P/I/D 状态，便于观察；它也会增加控制周期中的发布工作。

### 9. 正运动学和里程计是什么关系？

正运动学使用四个**实测**轮速恢复 `base_link` 速度：

```text
vx = r/4 * ( w_FL + w_FR + w_BL + w_BR)
vy = r/4 * (-w_FL + w_FR + w_BL - w_BR)
wz = r/(4k) * (-w_FL + w_FR - w_BL + w_BR)
```

控制器再把车体速度旋转到 `odom` 坐标系，并按周期积分得到位置和朝向。它是轮速估计，轮子打滑或几何参数不准都会让估计漂移；不要把里程计说成绝对真值。

### 10. `/odom` 和 TF 为什么都要发布？

`nav_msgs/Odometry` 是一条消息，携带估计的位姿和速度；TF 是坐标树中的变换，方便其他 ROS 节点把传感器、地图和机器人数据转换到同一坐标系。这里两者的位姿都表达 `odom -> base_link`，必须使用相同时间戳、位置和朝向。

### 11. global 模式为什么需要 TF？

`geometry_msgs/Twist` 没有 `frame_id`，单看消息不知道速度是相对 `base_link` 还是 `odom/map`。配置 `command_mode` 和 `world_frame_id` 明确约定后，控制器用 `transformVector()` 把世界坐标系的平移速度旋转到车体坐标系，再计算轮速。向量变换只旋转，不添加坐标原点平移。

### 12. 插件是怎样加载的？

按这个顺序理解：C++ 导出宏注册类 → 插件 XML 给出库和类名 → `package.xml` 导出 XML → YAML 的 `type` 选中插件 → launch 中的 `controller_manager` spawner 请求加载。名称任一处不一致都可能造成插件找不到。

### 13. 为什么有 `RealtimeBuffer` 和实时发布器？

ROS 订阅回调和控制循环不是同一个时序。回调把最新命令写入 `RealtimeBuffer`，`update()` 在控制周期中读取它，避免在实时循环里做普通 ROS 通信。实时发布器同样用于把发布工作安排得更适合控制循环。

### 14. 最大轮速、加速度限制和超时各解决什么问题？

- 最大轮速：若逆解结果中任一轮超限，就按统一比例缩放四轮，尽量保留运动方向。
- 加速度限制：限制每个周期目标轮速的变化量，让目标变化更平缓。
- 命令超时：一段时间收不到新 `/cmd_vel` 后，把期望速度置零。

这些功能是否达到合适的实际效果，要在仿真中展示并记录，不能仅凭代码存在就算验证通过。

## 运行前必须补上的实测答案

- `rm_description_for_task` 两个仓库是否都在 `master`，参考控制器是否由 CLion 成功构建并在 Gazebo 运行：`[填运行证据]`
- Hero Xacro 中实际轮半径、轮心相对 `base_link` 的 x/y、关节轴方向：`[填 Xacro 行和计算过程]`
- 四种动作的轮速目标/反馈曲线，以及最终 PID 参数：`[填 rqt_plot / rqt_reconfigure 记录]`
- `/odom` 和 `odom -> base_link` TF 是否一致：`[填命令输出]`
- global 模式是否在机器人转向后仍沿世界坐标目标方向移动：`[填 Gazebo 录屏结果]`
- CLion attach 到 `gzserver` 的断点、变量和实际排错经历：`[填截图与说明]`

## 参考源码

- [参考控制器实现](https://github.com/gdut-dynamic-x/simple_chassis_controller/blob/master/src/simple_chassis_controller.cpp)
- [参考控制器类定义](https://github.com/gdut-dynamic-x/simple_chassis_controller/blob/master/include/simple_chassis_controller/simple_chassis_controller.h)
- [ROS Noetic `control_toolbox::Pid` 实现](https://github.com/ros-controls/control_toolbox/blob/noetic-devel/src/pid.cpp)
- [考核细则](https://github.com/Bluefairy-ljy/simple_chassis_controller/blob/master/doc/requirement.md)
