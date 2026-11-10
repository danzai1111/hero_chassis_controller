# Hero 麦轮几何与方向核对指南

`controllers.yaml` 当前的 `wheel_radius`、`wheel_x_offset`、`wheel_y_offset` 只是待核实的候选值。几何错误会同时影响逆运动学、正运动学、PID 目标和里程计；先用指定 `rm_description_for_task` 的模型证据确认，不能靠调 PID 弥补。

## 1. 确认仓库、分支和源文件

先确认用的是考核指定仓库的 master，并定位 Hero 模型文件：

```bash
cd ~/catkin_ws/src/rm_description_for_task
git status --short --branch
git rev-parse HEAD
grep -RIn -E 'left_front_wheel_joint|right_front_wheel_joint|left_back_wheel_joint|right_back_wheel_joint' . \
  --include='*.xacro' --include='*.urdf' --include='*.launch'
grep -RIn -E 'wheel_radius|radius=|cylinder|mesh filename' . \
  --include='*.xacro' --include='*.urdf'
```

把仓库 commit、匹配的文件路径和行号记下来。Xacro 可能通过宏参数或 `<xacro:property>` 间接设置半径和坐标；看到变量名后要继续查它的定义和传入值，直到找到展开后的数值。

## 2. 优先检查展开后的 URDF

仿真 launch 通常会先把 Xacro 展开并写入参数服务器。启动指定 Hero 模型后，查找实际参数名：

```bash
rosparam list | grep robot_description
```

常见名称是 `/robot_description`。确认后可导出展开结果：

```bash
rosparam get /robot_description > /tmp/hero_expanded.urdf
grep -n -A12 -B4 -E 'left_front_wheel_joint|right_front_wheel_joint|left_back_wheel_joint|right_back_wheel_joint' /tmp/hero_expanded.urdf
```

如果 `robot_description` 带命名空间，用 `rosparam list` 显示的实际名称替换。展开文件能帮助发现最终值，但报告仍要记录这些值来自哪个指定仓库的 Xacro、宏参数或模型配置。

## 3. 逐轮确定关节名和轮心位置

每个 wheel joint 都要记录：joint 名称、parent link、child link、`origin xyz/rpy`、`axis xyz`，以及轮子碰撞/几何中心相对 child link 的偏移。先做四行表：

| 位置 | joint 名称 | parent link | child link | joint origin | 轮心相对 child link |
| --- | --- | --- | --- | --- | --- |
| 前左 FL | `[填写]` | `[填写]` | `[填写]` | `[填写]` | `[填写]` |
| 前右 FR | `[填写]` | `[填写]` | `[填写]` | `[填写]` | `[填写]` |
| 后左 BL | `[填写]` | `[填写]` | `[填写]` | `[填写]` | `[填写]` |
| 后右 BR | `[填写]` | `[填写]` | `[填写]` | `[填写]` | `[填写]` |

URDF joint 的 `origin` 描述 child link 的坐标系相对 parent link 的变换。如果 parent 不是 `base_link`，需要沿 parent 链逐段组合变换；存在非零 `rpy` 时，不能简单把各段 `xyz` 相加。还要查看 wheel link 中 collision/visual 几何自己的 `origin`，避免把 link 原点误当成轮心。

把四个轮心最终表达在 `base_link` 坐标系中，确认前后轮 x 坐标的绝对值应一致或解释其差异，左右轮 y 坐标的绝对值应一致或解释其差异。配置使用正的绝对偏置：

```text
wheel_x_offset = |轮心在 base_link 下的 x|
wheel_y_offset = |轮心在 base_link 下的 y|
```

控制器里的旋转力臂 `k = wheel_x_offset + wheel_y_offset`，对应矩形布置麦轮的半长与半宽之和。不能误填成前后轴距或左右轮距的完整距离。

## 4. 确认轮半径

优先从指定模型最终展开的轮子碰撞几何、Xacro 宏参数或明确的 Hero 模型参数中确认有效滚动半径。若碰撞使用 mesh，不要把 mesh 的最大外接尺寸直接当成半径；继续查 mesh 单位、缩放和建模轴向，并确认滚动半径定义。若模型把麦轮滚子单独建模，查清底盘模型采用的有效轮半径。

写入证据表：

| 项目 | 来源文件/行号或参数名 | 取值 | 单位 | 如何确认 |
| --- | --- | ---: | --- | --- |
| wheel radius | `[填写]` | `[填写]` | m | `[说明碰撞几何、宏参数或模型标定]` |
| wheel x offset | `[填写]` | `[填写]` | m | `[说明从 base_link 到轮心的变换]` |
| wheel y offset | `[填写]` | `[填写]` | m | `[说明从 base_link 到轮心的变换]` |

## 5. 关节轴方向要用仿真确认

URDF 的 `axis xyz` 是相对 joint 坐标系给出的方向；若 joint 有旋转，最终轴向还要结合 `origin rpy` 和 parent 链变换。不能只看 `axis xyz` 的正负就断定车轮在 Gazebo 中的正转方向。

先核对 C++/YAML 使用的顺序固定为 FL、FR、BL、BR，再用小幅度指令验证：

| 指令 | 期望目标轮速符号（FL, FR, BL, BR） | 必须观察 |
| --- | --- | --- |
| `linear.x > 0` 前进 | `+ + + +` | 机器人确实向前，四个实测轮速能跟随 |
| `linear.y > 0` 左移 | `− + + −` | 机器人确实向左，横移未被轮向抵消 |
| `angular.z > 0` 逆时针 | `− + − +` | 机器人确实逆时针旋转 |

如果符号计算符合公式但机器人方向不对，先核查关节顺序、joint 轴、轮子 roller 布置和模型坐标链。确认原因后再修正符号/配置；不要先用大 PID 或不一致的轮径掩盖方向问题。

## 6. 同步配置和记录

几何核实后，只把确认值写入 `config/controllers.yaml`，并在 `simulation_log.md` 记下 Xacro 路径、commit、行号、变换累加过程和方向测试证据。查看 `git diff`，再编译并逐项验证前进、左移、旋转、`/odom` 与 TF。

如果找不到某个宏参数、parent 链或 mesh 缩放，把对应 Xacro 片段和四轮 joint 表贴给指导者一起算；保留候选值并暂停基于这些值的调参与汇报。
