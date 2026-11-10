# 四轮 PID 动态调参与曲线记录

本指南按评分项帮助你实际使用 `rqt_reconfigure` 和 `rqt_plot`。配置中的 `p/i/d` 是启动值，不等于已经调好的参数。最终数值、曲线和现象都要来自 Ubuntu/Gazebo 的真实运行。

## 先理解闭环里的三个量

每个轮子都有一个目标角速度和一个关节反馈的实测角速度：

```text
误差 = 目标角速度 - 实测角速度
PID 输出力矩 = control_toolbox::Pid(误差, 控制周期)
```

- `p` 依据当前误差立即修正。太小会跟随慢；太大可能振荡或超调。
- `i` 累积长期误差，可减少持续偏差；太大容易积累过量并引起超调。
- `d` 依据误差变化速度提供阻尼；速度反馈有噪声时，D 项可能放大抖动。
- `i_clamp` 限制积分项。最终力矩还会被 `max_effort` 限幅；若输出长期顶住限幅，继续增加增益通常不会解决饱和。

先调 P，再根据稳态表现少量加入 I；只有确实需要阻尼且曲线足够平滑时再试 D。每轮逐步改参数并记录，不要一次把参数调到很大。

## 第一步：启动并确认观测接口

1. 确认 `wheel_radius`、轮心偏置、关节名和关节正方向已从 Hero Xacro 核实，并已在 Gazebo 中确认前进/左移/旋转方向。
2. 运行 `hero_chassis_controller` 仿真 launch，确认控制器成功启动。
3. 另开终端执行：

   ```bash
   rostopic list | grep -E 'wheel_state|/odom'
   rostopic echo -n 1 /controller/hero_chassis_controller/wheel_state
   rosservice list | grep set_parameters
   ```

   预期看到控制器的 `wheel_state` 话题，以及动态参数服务。实际命名空间以你的 `rosnode list`、`rostopic list` 和 `rosservice list` 输出为准；若和下面示例不同，按真实名称替换。

`wheel_state` 的 `Float64MultiArray.data` 下标固定为：

| 下标 | 含义 |
| ---: | --- |
| 0 / 1 | 前左轮目标 / 实测角速度 |
| 2 / 3 | 前右轮目标 / 实测角速度 |
| 4 / 5 | 后左轮目标 / 实测角速度 |
| 6 / 7 | 后右轮目标 / 实测角速度 |

单位均为 `rad/s`。这组数据由控制器在 `update()` 中写入，便于把四轮目标值和关节反馈放在同一张图上比较。

## 第二步：在 rqt_reconfigure 找四个 PID

在已 source ROS Noetic 和工作区环境的终端运行：

```bash
rosrun rqt_reconfigure rqt_reconfigure
```

在参数树中展开控制器对应的命名空间，再展开 `pid`。应能找到四组关节参数：

```text
/controller/hero_chassis_controller/pid/left_front_wheel_joint
/controller/hero_chassis_controller/pid/right_front_wheel_joint
/controller/hero_chassis_controller/pid/left_back_wheel_joint
/controller/hero_chassis_controller/pid/right_back_wheel_joint
```

每组里查看 `p`、`i`、`d`、`i_clamp`。如果实际树状名称不同，不要凭示例猜；以 `rosservice list` 和界面为准。改变参数后先看该轮速度曲线如何变化，并将旧值、新值和现象写进 `simulation_log.md`。

## 第三步：在 rqt_plot 看目标和实测

最直接的检查方式是对一个轮子同时画目标与实测，例如前左轮：

```bash
rosrun rqt_plot rqt_plot \
  '/controller/hero_chassis_controller/wheel_state/data[0]' \
  '/controller/hero_chassis_controller/wheel_state/data[1]'
```

如果界面里没有自动显示曲线，在 rqt_plot 的 Topic 输入栏分别添加这两条数据路径。也可以按上表添加另外三轮的目标/实测对。方括号已经放在引号里，避免 shell 将它当作通配符。

观察：目标曲线变化后，实测曲线是否跟上；两者之间是否有长期偏差；实测是否明显超调、振荡；轮速目标是否被 `max_wheel_speed` 限制；目标斜率是否受 `max_wheel_acceleration` 限制。

## 第四步：发一条短的 body 模式测试命令

先只使用很小的前进速度观察，例如 `0.1 m/s`：

```bash
rostopic pub -r 10 /cmd_vel geometry_msgs/Twist \
  '{linear: {x: 0.1}}'
```

停止发送时在该终端按 `Ctrl+C`。控制器设置了 `command_timeout`；目标速度会在超时后归零，同时按配置的轮速加速度限制减速。不要在 PID 尚未稳定或轮子方向未确认时发送大速度指令。

这条命令用于形成可观察的速度阶跃，不代表已经确认参数。把你实际使用的指令、曲线和现象记录下来。

## 第五步：循序调参

1. 记录当前 YAML 的四轮参数，以及测试指令和初始曲线。
2. 先保持 `i=0`、`d=0`，小步调整 `p`。观察跟随速度和振荡；每次变化都记数值和现象。
3. 如果 P 已能稳定跟随但仍有持续偏差，可少量增加 `i`，并保证 `i_clamp` 为适合当前力矩范围的正值。
4. 出现超调或持续振荡时，先回退最近一次增益改动；不要同时大改 P、I、D。
5. 仅在观察到可重复的超调且速度测量平滑时尝试小 D，并留意 D 对噪声的放大。
6. 检查 PID 输出是否长期达到 `±max_effort`。力矩饱和时，先检查几何、轮子方向、速度上限和负载，再判断增益。
7. 对四个轮子分别核对目标和实测曲线。四轮最终值可以不同，但每个不同值都应有对应观察依据。

## 第六步：保存、复验和留证

`rqt_reconfigure` 改的是运行中的参数服务器和 PID 实例，不会自动编辑仓库里的 `config/controllers.yaml`。把最终确认的四轮参数手动抄回 YAML，重启控制器，再用相同指令复验。这样才能证明仓库里的配置也能复现结果。

在 `doc/simulation_log.md` 中记录：日期、仿真命令、测试速度、四轮 P/I/D/`i_clamp`、每次改动理由、曲线文件、饱和或振荡现象，以及最终复验结果。保存 rqt_reconfigure 参数界面、rqt_plot 曲线和 `/odom` 对应输出，作为考核视频与仓库证据。

如果 rqt 工具、动态参数服务或曲线数据没有出现，先保存终端完整报错与以下输出，不要先安装或修改配置：

```bash
rosnode list
rostopic list
rosservice list | grep set_parameters
```
