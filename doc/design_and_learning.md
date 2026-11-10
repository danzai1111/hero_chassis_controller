# 设计推导与学习笔记

本文按“坐标约定 → 逆解 → PID → 正解与积分 → TF → 插件加载”的顺序，解释控制器的主要思路，方便逐段阅读源码和准备答辩。

## 1. 坐标约定

在 `base_link` 中使用 ROS 常见平面约定：x 向前、y 向左、z 向上；正 `angular.z` 表示逆时针旋转。四个轮子的数组顺序固定为：

```text
0 front-left, 1 front-right, 2 back-left, 3 back-right
```

轮子实际转向若与这个符号约定不一致，应先检查模型的关节轴和轮子命名。不要在 PID 参数里掩盖符号错误。

## 2. 麦克纳姆轮逆运动学

设车体速度为 `(vx, vy, wz)`，轮半径为 `r`，轮心相对底盘中心的纵向/横向绝对距离为 `a`、`b`，令 `k = a + b`。按当前示例模型的轮子顺序，计算：

```text
w_FL = ( vx - vy - k*wz) / r
w_FR = ( vx + vy + k*wz) / r
w_BL = ( vx + vy - k*wz) / r
w_BR = ( vx - vy + k*wz) / r
```

### 公式的直观来源

轮子角速度乘轮半径得到轮缘线速度：`v_wheel = r * omega`。麦轮滚子允许一个斜向分量自由滚动，因此每个轮子的接触约束把车体的前后速度 `vx` 和左右速度 `vy` 以不同符号组合起来：本控制器采用的滚子/关节约定中，FL、BR 行使用 `vx - vy`，FR、BL 行使用 `vx + vy`。

车体绕 z 轴转动时，位置为 `(x_i, y_i)` 的轮心具有平移速度 `(-wz*y_i, wz*x_i)`。结合四轮位置和麦轮滚子方向，转动项对四轮分别变成 `-k*wz, +k*wz, -k*wz, +k*wz`，其中 `k=a+b`。把平移项和转动项相加，再除以半径 `r`，就得到四个目标角速度。

矩阵写法能看清每个符号来自哪里：

```text
                 [ 1  -1  -k ] [ vx ]
[w_FL,w_FR,      [ 1   1   k ] [ vy ]
 w_BL,w_BR]^T = 1/r * [ 1   1  -k ] [ wz ]
                 [ 1  -1   k ]
```

逆解的输入 `(vx, vy, wz)` 单位分别为 `m/s, m/s, rad/s`；矩阵里 `k*wz` 单位为 `m/s`；除以轮半径（米）后得到轮角速度 `rad/s`。

正运动学是对同一矩阵做线性反解。四轮速度相加时，`vy` 和转动项正负抵消，剩下 `4*vx/r`；按 `(-,+,+,-)` 加权时只留下 `4*vy/r`；按 `(-,+,-,+)` 加权时只留下 `4*k*wz/r`。因此：

```text
vx = r/4   * ( w_FL + w_FR + w_BL + w_BR)
vy = r/4   * (-w_FL + w_FR + w_BL - w_BR)
wz = r/(4k)* (-w_FL + w_FR - w_BL + w_BR)
```

只前进时四个目标轮速同号；只左移时为 `− + + −`；只逆时针旋转时为 `− + − +`。这是当前控制器使用的滚子和关节符号约定。不同模型的轮子滚子排列、joint 轴方向可能不同，必须再对照指定 Xacro 并在 Gazebo 中小幅验证；代数上正逆解互相抵消，只能证明两组公式互相匹配，不能单独证明符号适配真实机器人。

符号可以用三个简单动作自检：

| 指令 | 前左 | 前右 | 后左 | 后右 |
| --- | --- | --- | --- | --- |
| 前进 `vx > 0` | + | + | + | + |
| 左移 `vy > 0` | − | + | + | − |
| 逆时针 `wz > 0` | − | + | − | + |

源码中先计算四个轮速；如果最大轮速超过 `max_wheel_speed`，四个值一起按同一比例缩小，这样不会改变运动方向。随后可选地按 `max_wheel_acceleration` 限制每个控制周期的轮速变化。

## 3. 每轮 PID

一个轮子对应一个 `control_toolbox::Pid`。每个控制周期读取关节 `getVelocity()` 得到实测角速度 `w_actual`，目标角速度是 `w_target`：

```text
error = w_target - w_actual
effort = PID(error, dt)
```

PID 输出是力矩，写到 `EffortJointInterface::JointHandle::setCommand()`。这解释了为何本控制器使用力矩接口：速度闭环由本插件实现。`Pid::init()` 从 `pid/<joint>` 命名空间读取参数并启动动态重配置；`computeCommand(error, period)` 在 `update()` 中根据实际周期计算输出。积分限幅用于防止误差长期存在时积分项持续增长。

四组 PID 初值相同只是启动方便，不代表四个轮子的仿真响应完全相同。调参时先看期望值与反馈值，再小步调节 `p`；出现稳态误差时再加少量 `i`，有明显超调或振荡时先回退 `p`，再谨慎尝试 `d`。

`i_clamp` 限制积分项本身的最大贡献，不是限制积分误差；设为 0 时积分贡献会被夹在 0。当前配置把它暂设为 `2.0`，与 PID 输出的 `max_effort: 2.0` 同量级，后续应结合 Gazebo 响应调整。配置 `publish_state: true` 后，`control_toolbox::Pid` 还会发布对应的 `control_msgs/PidState`，可用于查看 P/I/D 内部状态；这和控制器单独发布的 `wheel_state` 目标/实测轮速不同。

本实现把 `computeCommand()` 的输出再限制到 `±max_effort`。PID 自己依据 `i_clamp` 限制积分项，但它不知道后续力矩裁剪的结果；因此调参时要观察是否长期顶在力矩上限。`publish_state: true` 便于学习和诊断，但 `control_toolbox::Pid` 文档提示发布状态可能增加实时循环开销；如果实测更新周期受影响，可在配置中关闭它。

## 4. 正运动学和里程计

令四轮实测角速度为 `w_FL, w_FR, w_BL, w_BR`，用正解还原 `base_link` 中的车体速度：

```text
vx = r/4 * ( w_FL + w_FR + w_BL + w_BR)
vy = r/4 * (-w_FL + w_FR + w_BL - w_BR)
wz = r/(4k) * (-w_FL + w_FR - w_BL + w_BR)
```

用当前暂定参数 `r = 0.07625 m`、`k = 0.3738 m` 做代数自检：

| 输入 `(vx, vy, wz)` | 逆解轮速 `(FL, FR, BL, BR)` rad/s | 正解恢复 `(vx, vy, wz)` |
| --- | --- | --- |
| `(0.1, 0, 0)` | `(1.3115, 1.3115, 1.3115, 1.3115)` | `(0.1, 0, 0)` |
| `(0, 0.1, 0)` | `(-1.3115, 1.3115, 1.3115, -1.3115)` | `(0, 0.1, 0)` |
| `(0, 0, 0.3)` | `(-1.4707, 1.4707, -1.4707, 1.4707)` | `(0, 0, 0.3)` |

这说明当前写下的正、逆运动学公式互相匹配。这里使用的是尚待 Xacro 确认的暂定几何数值；代数自检不能证明关节方向、真实几何或 Gazebo 运动正确。

每个周期用当前 yaw 的中点方向将车体平移速度转到 odom 坐标系，再乘以周期 `dt` 累加 `x/y/yaw`。中点积分比一直使用周期起点的朝向更适合同时平移和转弯。

积分结果写入 `nav_msgs/Odometry`。当 `publish_tf` 为真时，相同的 `x/y/yaw` 也组成 `odom -> base_link` 变换，保证 `/odom` 位姿和 TF 表达的是同一状态。

## 5. 世界坐标速度模式

`geometry_msgs/Twist` 没有坐标系字段，因此全局模式必须由配置中的 `world_frame_id` 明确指定输入坐标系（例如 `odom` 或 `map`）。回调把平移部分封装为 `Vector3Stamped`，调用 `tf::TransformListener::transformVector()` 变换到 `base_link`；向量变换只旋转，不加平移。变换结果进入实时缓冲区，控制循环仍只读取缓冲值。

旋转分量 `angular.z` 对平面底盘保持不变，因为绕 z 轴的角速度在两个水平坐标系中相同。若 TF 暂不可用，控制器打印节流警告，并把当前指令置零。

## 6. pluginlib 加载链路

1. C++ 文件末尾的 `PLUGINLIB_EXPORT_CLASS` 导出 `HeroChassisController`。
2. `hero_chassis_controller_plugins.xml` 声明库文件、插件名、具体类和基类。
3. `package.xml` 的 `<controller_interface plugin="..."/>` 让插件索引发现 XML。
4. `controllers.yaml` 的 `type` 使用 `hero_chassis_controller/HeroChassisController`。
5. launch 文件加载参数并调用 `controller_manager` 的 spawner，最终由 pluginlib 创建插件实例。

这几个字符串需要彼此一致。若控制器类型加载失败，依次核对 CMake 生成的 `libhero_chassis_controller.so`、插件 XML 的 `library path`、package.xml 导出和 YAML 的 `type`。

## 7. CLion 调试建议

在 Linux 那台电脑上用 CLion 打开 catkin 工作区。设置断点后启动 Gazebo 仿真，再把 CLion 调试器附加到 `gzserver` 进程；这是考核细则给出的调试方式。优先观察 `init()` 的关节句柄/PID 初始化、`commandCallback()` 的模式转换，以及 `update()` 的期望轮速、实测轮速、误差和力矩。

## 8. 答辩自测问题

1. `base_link` 的 x、y、z 各朝哪个方向？左移时四轮为什么是 `− + + −`？
2. 逆运动学输入和输出分别是什么量、单位是什么？
3. 为什么一个总 PID 不能替代四个独立轮速 PID？
4. 轮速反馈从哪里读取，PID 输出最终写到哪里？
5. `/odom` 中速度是车体坐标系还是世界坐标系？积分位姿属于哪个坐标系？
6. 为什么发布里程计后还要发 `odom -> base_link` TF？
7. `Twist` 自己为什么不能说明是 `map` 还是 `base_link` 坐标系？全局模式如何解决？
8. `pluginlib` 的导出宏、XML、package.xml、YAML 和 spawner 各自负责什么？

实际数值、调参过程和运行结果应由自己的仿真记录作答；不要把尚未运行的预期写成实验结论。
