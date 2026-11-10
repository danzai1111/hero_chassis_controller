# 从零开始：Linux / CLion 操作单

这份操作单先完成考核的 10 分必做项。按顺序执行；遇到第一处错误就停下来，把错误文字或截图发给指导者，不要连续尝试不确定的命令。

## 第 0 步：只检查环境

先在 Ubuntu 打开一个终端，逐行执行下面的命令。这一步不会安装、删除或改动文件，只确认 ROS、Gazebo 和 catkin-tools 是否能被找到：

```bash
source /opt/ros/noetic/setup.bash
printf 'ROS_DISTRO=%s\n' "$ROS_DISTRO"
command -v git || echo '没有找到 git'
command -v rosdep || echo '没有找到 rosdep'
command -v roscore || echo '没有找到 roscore'
command -v catkin || echo '没有找到 catkin-tools 命令'
command -v gzserver || echo '没有找到 Gazebo gzserver'
command -v clion || echo '终端里没有 clion 命令（可能仍可从应用菜单启动）'
```

正常情况下 `ROS_DISTRO` 应显示 `noetic`，并能找到 `git`、`rosdep`、`roscore` 和 `gzserver`。这些命令中有缺失时先停下来，把完整输出发来，我们再判断。终端里找不到 `catkin` 时也先别自行安装；CLion 命令找不到时可从应用菜单启动。`source` 是把 ROS 环境加载到当前终端，不会更改系统配置。

这一步的目的，是先区分“ROS 环境没加载”和“项目本身编译失败”。确认后再继续克隆参考工程。

## A. 获取并确认参考工程

在 Ubuntu 终端中：

```bash
source /opt/ros/noetic/setup.bash
mkdir -p ~/catkin_ws/src
cd ~/catkin_ws/src
git clone --branch master https://github.com/Bluefairy-ljy/rm_description_for_task.git
git clone --branch master https://github.com/gdut-dynamic-x/simple_chassis_controller.git
```

如果这两个目录已存在，不要重复克隆；先检查各自分支。克隆后在终端确认两个分支都显示 `master`：

```bash
git -C rm_description_for_task status --short --branch
git -C simple_chassis_controller status --short --branch
```

再安装工作区依赖：

```bash
cd ~/catkin_ws
rosdep install --from-paths src --ignore-src -r -y
```

## B. 在 CLion 编译参考控制器

`catkin_tools` 工作区没有总的顶层 `CMakeLists.txt`，JetBrains 建议在 CLion 中分别打开每个包的 `CMakeLists.txt`。所以先打开：

```text
~/catkin_ws/src/simple_chassis_controller/CMakeLists.txt
```

具体步骤：

1. 先在终端加载 ROS 环境，再从这个终端启动 CLion，这样 IDE 能继承 ROS 的环境变量：

   ```bash
   source /opt/ros/noetic/setup.bash
   cd ~/catkin_ws
   if [ -f devel/setup.bash ]; then source devel/setup.bash; fi
   /你的/CLion/安装目录/bin/clion.sh
   ```

2. 在 CLion 的 CMake 设置中使用 `Debug` 配置。为这个包设置独立构建目录，例如 `~/catkin_ws/clion-build/simple_chassis_controller`；CMake options 加上 `-DCATKIN_DEVEL_PREFIX:PATH=/home/你的用户名/catkin_ws/devel`。
3. 先用 CLion 的 **Build** 操作编译。如果之后需要 catkin-tools 负责生成完整 devel 空间，从 CLion 内置终端在工作区运行：

   ```bash
   catkin config --cmake-args -DCMAKE_BUILD_TYPE=Debug
   catkin build simple_chassis_controller
   ```

   这使用的是考核允许的 `catkin build`，不要使用 `catkin_make`。将 CLion 的 Build 和 Debug 配置保留为主要 IDE 工作流。
4. 以后打开 `hero_chassis_controller` 时，同样在 CLion 中单独打开它的 `CMakeLists.txt`，并为它设置独立构建目录 `~/catkin_ws/clion-build/hero_chassis_controller`。

JetBrains 的 [ROS setup tutorial](https://www.jetbrains.com/help/clion/ros-setup-tutorial.html) 说明了 catkin-tools 工作区需逐包打开、让 CLion 从已 source 的环境启动；[Attach to Process 文档](https://www.jetbrains.com/help/clion/attach-to-process.html)介绍了 Linux 进程附加步骤。

## C. 运行并理解 `simple_chassis_controller`

编译成功后，在已加载 ROS Noetic 环境的终端运行：

```bash
source /opt/ros/noetic/setup.bash
source ~/catkin_ws/devel/setup.bash
roslaunch simple_chassis_controller run_simulation_and_controller.launch
```

Gazebo 窗口打开后，如果仿真暂停，点击播放按钮。先确认 Hero 底盘能在 Gazebo 中出现并且原控制器启动成功。

然后在 CLion 中打开 `simple_chassis_controller` 源码，在 `SimpleChassisController::update()` 的 `setCommand()` 前设置断点。这个函数会持续运行，仿真启动后附加进程仍能命中断点。`init()` 只在加载控制器时运行一次，通常在你 attach 之前就结束；先通过源码理解它的关节句柄初始化。

- `SimpleChassisController::init()`：看四个关节句柄如何从 `EffortJointInterface` 获取。
- `SimpleChassisController::update()`：看周期更新里如何调用 `setCommand()`。
- 文件末尾 `PLUGINLIB_EXPORT_CLASS(...)`：看 C++ 类如何导出为控制器插件。

在 CLion 中选择 **Run | Attach to Process**（Linux 快捷键 `Ctrl+Alt+F5`），搜索并选择 `gzserver`，点击附加，然后继续运行并观察断点和局部变量。若断点不可用，先确认包是 `Debug` 构建；若附加时报 `ptrace: Operation not permitted`，先把错误发来，不要自行修改系统安全设置。准备好回答：

1. 四个 `JointHandle` 分别对应哪个轮子？
2. `state_` 和 `last_change_` 在示例中控制什么？

逐成员说明、六种测试力矩组合和断点练习见 [`reference_controller_walkthrough.md`](reference_controller_walkthrough.md)。先在本地源码中核实再回答。
3. 当前示例给轮子发的是速度指令还是力矩指令？它和考核要求的四个速度 PID 有什么差别？
4. 插件 XML、YAML 中的 `type`、spawner 三者如何连起来？

## D. 用 `rm_template` 建个人仓库并导入新包

考核要求以 `rm_template` 为模板新建名为 `hero_chassis_controller` 的仓库。先在 GitHub 打开 [`gdut-dynamic-x/rm_template`](https://github.com/gdut-dynamic-x/rm_template)，点击 **Use this template** 创建你自己的 `hero_chassis_controller` 仓库。不要把模板仓库本身改名或直接推送到团队模板。

在 Ubuntu 克隆你刚创建的个人仓库到工作区（把占位符替换成自己的 GitHub 用户名）：

```bash
cd ~/catkin_ws/src
git clone https://github.com/你的GitHub用户名/hero_chassis_controller.git
```

把从当前对话所在电脑传来的 `hero_chassis_controller.zip` 放进 Ubuntu 的 `~/Downloads/`，再将包文件覆盖导入到克隆目录根部。此操作会保留该仓库自己的 `.git` 历史：

```bash
mkdir -p ~/Downloads/hero_pkg_import
unzip ~/Downloads/hero_chassis_controller.zip -d ~/Downloads/hero_pkg_import
cp -a ~/Downloads/hero_pkg_import/hero_chassis_controller/. ~/catkin_ws/src/hero_chassis_controller/
```

核对导入位置和 Git 状态：

```bash
test -f ~/catkin_ws/src/hero_chassis_controller/package.xml && echo '包文件位置正确'
git -C ~/catkin_ws/src/hero_chassis_controller status --short --branch
git -C ~/catkin_ws/src/hero_chassis_controller remote -v
```

包清单应直接位于 `~/catkin_ws/src/hero_chassis_controller/package.xml`，不能多出一层 `hero_chassis_controller/hero_chassis_controller/`。在 CLion 中检查模板遗留的占位文件，确认用途后再删除无用文件；先把 `package.xml` 中的 `Student` 和 `student@example.com` 换成你自己的维护者姓名与邮箱。构建前检查 `git diff`，成功后再提交真实的导入和修改记录。若编译失败，发送第一条完整错误和对应源文件行号。

运行新包前，从 `rm_description_for_task` 的 Hero Xacro 核对轮半径以及四个轮心相对 `base_link` 的纵、横坐标。先只查文件，不急着改参数：

```bash
cd ~/catkin_ws/src/rm_description_for_task
grep -RIn -A12 -B4 -E 'left_front_wheel_joint|right_front_wheel_joint|left_back_wheel_joint|right_back_wheel_joint' . --include='*.xacro' --include='*.urdf'
grep -RIn -E 'wheel_radius|radius=' . --include='*.xacro' --include='*.urdf' | head -40
```

第一条找四个轮子的 joint 定义及附近的 parent/origin；第二条找半径定义。若 joint 的 parent 不是 `base_link`，轮心位置需要沿 parent 链累加，不能直接把单个 `origin xyz` 当作底盘偏置。把这两条命令输出贴给指导者，我们一起确认真实数值后再改 `config/controllers.yaml`。然后依次验证前进、左移、旋转、`/odom`、TF 和 global 模式。

如果 Xacro 使用宏、parent link 不是 `base_link`，或 wheel 几何另有偏移，请按 [`geometry_verification_guide.md`](geometry_verification_guide.md) 追到展开后的参数，并逐段组合坐标变换。

## 当前需要你实际完成的部分

- 两个参考仓库的 master 分支状态。
- CLion 构建参考控制器的成功记录。
- Gazebo 实际启动画面。
- CLion attach 到 `gzserver` 后的断点或调试截图。
- Hero Xacro 中轮半径和轮心偏置的真实数值。
- 新包在你的 ROS Noetic 工作区中的编译/运行结果。

这些都是评分证据，当前共享工作区无法代替另一台电脑上的真实运行记录。
