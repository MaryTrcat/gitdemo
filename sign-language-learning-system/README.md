# 基于 Qt 与计算机视觉的手语动作学习系统

这是一个 Qt + C++ + MySQL 的毕业设计初版系统，按照八个模块组织代码：

1. 用户管理模块
2. 手势资源库模块
3. 摄像头视频采集模块
4. 手部关键点识别模块
5. 手势动作评分模块
6. 智能纠错反馈模块
7. 学习数据统计模块
8. 考核评测与错题复习模块

当前版本优先完成系统骨架、MySQL 数据库连接、练习闭环和 C++ 评分算法。手部关键点识别模块暂时使用模拟检测器，后续可替换为 MediaPipe Hands 或 OpenCV DNN。

## 构建方式

使用 Qt Creator 打开本目录的 `CMakeLists.txt`，选择带有 Qt 6 Widgets、SQL、Multimedia 模块的 Kit 构建运行。

也可以使用命令行：

```powershell
cmake -S . -B build
cmake --build build
```

## MySQL 配置

1. 创建数据库：

```sql
CREATE DATABASE sign_language_learning DEFAULT CHARACTER SET utf8mb4;
```

2. 执行 `database/schema.sql`。
3. 启动程序后在界面顶部填写主机、数据库、用户名、密码，点击“连接 MySQL”。

如果 Qt 提示缺少 MySQL 驱动，需要安装或编译 Qt 的 `QMYSQL` 驱动插件。

## 算法测试

```powershell
cmake --build build --target GestureScorerTests
.\build\GestureScorerTests.exe
```

