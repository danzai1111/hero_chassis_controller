# 参考控制器逐段阅读与调试练习

本练习对应 [`gdut-dynamic-x/simple_chassis_controller`](https://github.com/gdut-dynamic-x/simple_chassis_controller/tree/master) 当前 master 的头文件、源文件、YAML 和 launch。master 分支可能更新；在 Ubuntu 上请以你克隆下来的实际源码为准，并先确认分支。

## 先看类定义：数据放在哪里

打开 `include/simple_chassis_controller/simple_chassis_controller.h`：

- `SimpleChassisController` 继承 `controller_interface::Controller<hardware_interface::EffortJointInterface>`。这告诉 `controller_manager`：该插件需要力矩命令接口。
- `init(...)` 是控制器初始化入口；`update(time, period)` 是控制器管理器按仿真控制周期反复调用的更新入口。
- `front_left_joint_`、`front_right_joint_`、`back_left_joint_`、`back_right_joint_` 是四个 `JointHandle`。句柄代表某个关节的控制接口；本例用 `setCommand()` 给它写力矩。
- `state_` 是当前动作编号，取值在 0 到 5 之间。
- `last_change_` 保存上次切换动作时的 ROS 时间，用于判断是否已经过了两秒。

在 CLion 中给 `state_` 和 `last_change_` 加到 Watches。注意 `init()` 只在控制器加载时调用一次；`update()` 会持续周期执行，所以演示调试通常在 `update()` 下断点。

## 再看 `init()`：句柄与关节名如何对应

打开 `src/simple_chassis_controller.cpp` 的 `SimpleChassisController::init()`，逐个对应以下关系：

| 成员句柄 | 模型关节名 | 轮子位置 |
| --- | --- | --- |
| `front_left_joint_` | `left_front_wheel_joint` | 前左 |
| `front_right_joint_` | `right_front_wheel_joint` | 前右 |
| `back_left_joint_` | `left_back_wheel_joint` | 后左 |
| `back_right_joint_` | `right_back_wheel_joint` | 后右 |

`effort_joint_interface->getHandle(name)` 根据关节名向硬件接口索取句柄。如果模型或硬件接口没有注册这个名字，初始化会失败。这个简化参考代码没有捕获异常，也没有读取 YAML 中的关节名列表；它把四个名称直接写在 C++ 里。

练习：在 `getHandle()` 处设置断点或逐行调试，记下四个传入的字符串。再对照模型里的 joint 名称。实际名称要匹配。

## 看 `update()`：状态机和输出是什么

源文件的 `update()` 中定义了 `tau = 0.2`，并用 `cmd_[6][4]` 保存六种动作，每行四个值对应 FL、FR、BL、BR。随后，程序比较当前 ROS 时间 `time` 和 `last_change_`：超过两秒就把 `state_` 加一并对 6 取模，再把当前行的四个数分别写给四个句柄。

| `state_` | 注释动作 | FL、FR、BL、BR 命令 | 量的含义 |
| ---: | --- | --- | --- |
| 0 | 前进 | `+0.2, +0.2, +0.2, +0.2` | 四轮力矩 |
| 1 | 后退 | `-0.4, -0.4, -0.4, -0.4` | 四轮力矩 |
| 2 | 左移 | `-0.2, +0.2, +0.2, -0.2` | 四轮力矩 |
| 3 | 右移 | `+0.4, -0.4, -0.4, +0.4` | 四轮力矩 |
| 4 | 顺时针 | `+0.4, -0.4, +0.4, -0.4` | 四轮力矩 |
| 5 | 逆时针 | `-0.2, +0.2, -0.2, +0.2` | 四轮力矩 |

这里的 `cmd_` 是力矩命令，不是角速度。代码注释也明确说明这些是临时测试动作，正式底盘控制应使用逆运动学。`time` 被用于动作计时；这个示例没有使用 `period`。

练习：在 `if ((time - last_change_).toSec() > 2)` 行设断点，查看 `time`、`last_change_`、`state_`。继续运行并在时间变化后再次停住，观察 `state_` 如何从 0 走到 1，再继续到 5 后回到 0。Gazebo 的 ROS 仿真时间若在控制器加载前已走过两秒，首次更新可能马上切换状态；所以以断点观察值为准。

## 从 launch 到 C++ 实例：插件如何加载

把以下四处连起来：

1. `config/controllers.yaml` 将 `controller/simple_chassis_controller/type` 写成 `simple_chassis_controller/SimpleChassisController`。
2. `launch/run_simulation_and_controller.launch` 加载 YAML，并让 `controller_manager` 的 spawner 请求启动 `controller/simple_chassis_controller`。
3. `package.xml` 的 `<controller_interface plugin="..."/>` 注册插件 XML。
4. 插件 XML 把插件名映射到 `simple_chassis_controller::SimpleChassisController` 和共享库；C++ 文件末尾的 `PLUGINLIB_EXPORT_CLASS` 导出具体类。

可以把它理解为：spawner 请求一个 YAML 里指定的插件名，pluginlib 按包的导出索引找到 XML，再按 XML 找到共享库和 C++ 类，最后由 `controller_manager` 创建并初始化控制器。

调试练习：在 YAML 中圈出 `type`；在 launch 中圈出加载 YAML 的 `rosparam` 和 spawner 的 `args`；在 C++ 中圈出导出宏。答辩时分别说出它们的职责，不要把 XML、YAML 和 spawner 混为一件事。

## 用自己的话回答

1. 为什么本例继承 `Controller<EffortJointInterface>`，而没有继承速度接口？
2. `JointHandle` 中写进去的值是什么物理量？
3. `state_` 和 `last_change_` 如何配合？
4. 这段代码与最终四轮速度 PID 的主要区别是什么？
5. YAML 的插件 `type`、插件 XML、package.xml 导出、spawner 各自做什么？

参考答案：本例通过力矩接口执行固定力矩测试；它没有接收 `/cmd_vel`、没有将车体速度做逆解、没有比较期望/实测轮速，也没有调用 `control_toolbox::Pid`。新包的速度闭环才执行“底盘速度 → 目标轮速 → PID 力矩 → 关节反馈”。

## 必做项证据

在 Ubuntu/CLion/Gazebo 中真实完成后，保存：

- 参考包 CLion Debug 构建成功的画面。
- Gazebo 中 Hero 模型和参考控制器启动画面。
- CLion attach 到 `gzserver` 后，`update()` 断点命中并显示 `state_`、`last_change_` 或命令值的画面。
- 用自己的话回答上面的五个问题，写入 `simulation_log.md`。

本练习文档不证明已经完成构建、运行或调试；这些必须有你那台电脑上的实际证据。
