# 考核完成度清单

状态说明：**已写**表示文件里有对应实现；**待验证**表示必须在你那台 Ubuntu/CLion/Gazebo 电脑上实际检查；**待完成**表示尚无相应证据或交付物。不要把“已写”当成“仿真通过”。

视频交付要求：录制 10–15 分钟 `.mp4`，必须完整运行并演示代码功能；按“姓名 年级”命名，发送至 `y3099568863@sina.com`，截止时间为 **2026 年 11 月 12 日 24:00**。考核页要求使用团队统一 PPT 模板；模板尚未收到，拿到后再套用。

| 考核项 | 分值 | 当前状态 | 要留下的证据 |
| --- | ---: | --- | --- |
| 运行 `rm_description_for_task` 的 master 分支 | 2 | 待完成 | CLion 构建记录和 Gazebo 运行截图/录屏 |
| 学习、编译、运行并调试 `simple_chassis_controller` | 8 | 待完成 | CLion 编译结果；attach 到 `gzserver` 的断点/变量演示；能解释类成员和 pluginlib 加载链路 |
| 从 `rm_template` 创建 `hero_chassis_controller` 包 | 5 | 本地源码已准备；待按模板创建仓库并验证 | 新仓库名称、CMakeLists、package.xml、README |
| 版本管理 | 5 | 待完成 | 有意义的提交记录、分支合并记录；若有冲突，保留冲突解决前后记录 |
| ROS/Google C++ 代码规范 | 10 | 已设定 Google 风格 80 列并完成源码行宽检查；待 CLion 格式化和编译 | CLion 格式化后的源码、无明显警告的构建 |
| 四个 `control_toolbox::Pid`、配置和动态调参 | 20 | 已写，待调参验证 | `rqt_reconfigure` 参数调整、`rqt_plot` 期望/实测轮速曲线、各 PID 的 `PidState` 状态 |
| 麦轮逆运动学和 `/cmd_vel` | 15 | 已写，待方向验证 | 前进、左移、旋转三种指令的轮速符号和 Gazebo 运动 |
| 实测轮速正解、`/odom`、`odom -> base_link` | 15 | 已写，待运动验证 | `rostopic echo /odom` 与 `tf_echo` 结果 |
| TF 全局速度模式 | 15 | 已写，待场景验证 | 机器人转向后仍沿世界坐标指定方向运动的录屏 |
| 特色功能（加速度限制、键盘控制） | 5 | 已写，待演示 | 配置效果、键盘遥控录屏 |
| 10–15 分钟汇报视频 | 必交 | 待完成 | 按团队模板制作 PPT 并录制实际运行代码的 `.mp4`；按“姓名 年级”命名并在截止时间前发送；不能用静态截图代替运行演示 |

## 运行核对顺序

建议先复制 [`simulation_log_template.md`](simulation_log_template.md) 为自己的 `simulation_log.md`，每完成一项就记录实际命令、结果和证据文件名。
时间安排见 [`deadline_plan.md`](deadline_plan.md)，最后两天留给录制、复查和发送视频。

1. 确认 `rm_description_for_task` 和 `simple_chassis_controller` 都在 master 分支。
2. 先独立运行参考 `simple_chassis_controller`，理解类成员、plugin XML、YAML 和 spawner。
   逐段练习见 [`reference_controller_walkthrough.md`](reference_controller_walkthrough.md)。
3. 再编译本包，核实轮半径/轮心偏置/关节名，并启动底盘仿真。参数查证步骤见 [`geometry_verification_guide.md`](geometry_verification_guide.md)。
4. 先调 body 模式下的三种基础动作和 PID，再验 `/odom` 与 TF。
   PID 调参与 rqt 操作步骤见 [`pid_tuning_guide.md`](pid_tuning_guide.md)。
5. 切到 global 模式测试坐标变换，最后演示超时保护、加速度限制和键盘控制。
6. 根据真实运行记录完善 README、提交历史和汇报视频。
