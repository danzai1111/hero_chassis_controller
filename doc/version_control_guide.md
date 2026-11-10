# Git 版本管理操作单

本考核给版本管理 5 分。提交记录应反映真实开发过程；不要编造作者、组员、测试结果或冲突经历。你是单人组时，贡献说明如实写“独立完成”。

## 1. 创建考核仓库

按要求从 [`rm_template`](https://github.com/gdut-dynamic-x/rm_template) 创建 GitHub 仓库，仓库名使用 `hero_chassis_controller`。将 `hero_chassis_controller.zip` 中包目录的内容放入仓库根目录，检查并删除仍属于模板的空文件/占位说明，替换 `package.xml` 中的维护者姓名和邮箱。

保留以下资料：

- `README.md`：功能、接口、配置、构建与运行说明。
- `CMakeLists.txt`、`package.xml`：包构建、依赖和元信息。
- `include/`、`src/`、`config/`、`launch/`、`doc/`：源码、参数、启动文件和学习/验证记录。
- `.gitignore`：不提交 `build/`、`devel/`、`install/` 等构建产物。

## 2. 真实开发中的提交

先导入包并作一条清楚的初始提交；之后每完成并实际核验一项改动，就单独提交。可以参考这些消息格式：

```text
feat: add four-wheel velocity PID control
feat: calculate mecanum wheel targets from cmd_vel
feat: publish wheel-feedback odometry and odom tf
feat: transform global velocity commands through tf
fix: tune wheel geometry and pid gains for Hero simulation
docs: record CLion debugging and simulation evidence
```

提交说明写实际完成的内容。若某项尚未在 Gazebo 验证，就不要用 `fix` 或 `verified` 暗示验证通过。提交前查看 `git status` 和 `git diff`，避免加入构建产物、个人路径或未完成的调试改动。

## 3. 分支与合并

后续每个可独立验证的改动可从 `main` 开短期分支，例如 `feature/odom-tf` 或 `feature/global-command`，验证后合并回 `main`。单人项目的贡献说明保持真实，不虚构多人分工。

## 4. 练习并记录冲突解决

如果项目开发中没有自然产生冲突，可用一个明确标注的个人练习展示冲突解决能力，不要把练习说成多人协作冲突。两个练习分支必须从同一个 `main` 基线创建，且在创建完两边提交前不要合并；如果你的默认分支叫 `master`，下面命令中的 `main` 都换成 `master`。

1. 确认工作区干净并记下默认分支：

   ```bash
   git status --short
   git branch --show-current
   git switch main
   ```

2. 创建第一个分支，在 `doc/presentation_outline.md` 的“录制顺序”列表中选定一行，加入 body 模式演示细节并提交：

   ```bash
   git switch -c practice/body-doc
   # 用编辑器改选定的那一行
   git add doc/presentation_outline.md
   git commit -m "docs: add body mode demo details"
   ```

3. 回到原始 `main`，再从它创建第二个分支；编辑同一行，但补充 global 模式的演示细节并提交：

   ```bash
   git switch main
   git switch -c practice/global-doc
   # 用编辑器改同一行，写入不同内容
   git add doc/presentation_outline.md
   git commit -m "docs: add global mode demo details"
   ```

4. 合并第一个分支后，再合并第二个分支；第二次合并应出现冲突：

   ```bash
   git switch main
   git merge --no-ff practice/body-doc
   git merge --no-ff practice/global-doc
   ```

5. 在编辑器中解决同一行的冲突，保留两种模式的演示要点并删除 `<<<<<<<`、`=======`、`>>>>>>>` 标记。然后运行 `git status` 检查冲突状态，检查最终段落，再完成合并提交：

   ```bash
   git diff --check
   git add doc/presentation_outline.md
   git commit -m "docs: resolve practice merge conflict"
   git log --oneline --graph --decorate --all
   ```

6. 保留合并图、解决前后的 diff 和最终文件。汇报时说明这是个人的冲突处理练习，不是多人协作冲突。

若要完成这项评分且开发中没有自然冲突，可以按上述步骤做个人练习，并保留真实的分支、提交和解决记录。冲突发生时先判断两边改动分别想表达什么，再合并语义，不能只删除标记并丢掉一边内容。

## 5. 答辩时可以这样解释

- `main` 保存可运行的稳定版本；功能分支用于隔离一项改动。
- 合并前先检查差异；冲突标记表示 Git 无法判断两种文本应怎样合并。
- 解决冲突后要重新检查文档/代码并运行对应验证，再提交合并结果。
- 当前仓库是单人完成，因此 Git 作者和个人贡献都只写真实信息。
