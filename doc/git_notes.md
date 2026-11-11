# Git 开发记录

本仓库基于 `rm_template` 创建，保留了模板的提交历史，并在独立分支上完成控制器导入、参数修正和验证记录。

## 分支

```text
master
practice/body-doc
practice/global-doc
```

## 合并冲突练习

`practice/body-doc` 和 `practice/global-doc` 修改了同一行文档内容，合并时产生了真实文本冲突。最终解决时保留两种模式的说明。

该冲突属于单人完成的 Git 操作练习，不代表多人协作经历。

完整提交以及合并关系可以使用以下命令查看：

```bash
git log --oneline --graph --decorate --all
```
