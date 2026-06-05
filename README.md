# RK1808 人脸考勤系统

这是一个面向 Rockchip RK1808 Linux 平台的人脸考勤示例项目。系统使用摄像头采集实时画面，通过 RockX/RKNN 完成人脸检测、关键点对齐与人脸特征匹配，并在 DRM 屏幕界面上提供人脸录入、考勤打卡、管理员查看/删除记录和补卡等功能。

## 项目概览

- **目标平台**：RK1808/RK1808 Linux 设备
- **开发语言**：C
- **核心能力**：摄像头采集、DRM 显示、触摸交互、RockX 人脸识别、BMP 记录保存
- **运行方式**：设备端进入 `app-runtime` 目录后运行主程序 `f1`
- **模型依赖**：随仓库提供 `rockx-sdk-rk1808-linux`，包含 RockX 头文件、动态库与人脸模型数据

## 目录结构

```text
.
├── README.md
├── app-runtime/                 # 设备端可执行运行目录
│   ├── f1                       # 主程序可执行文件
│   ├── libcamera.so             # 摄像头封装动态库
│   ├── libDRMwrap.so            # DRM 显示封装动态库
│   ├── 1.bmp                    # 首页界面
│   ├── 2.bmp / 2_success.bmp / 2_fail.bmp
│   ├── 3.bmp / 3_success.bmp / 3_fail.bmp
│   ├── face/                    # 已录入人脸库与预览图片
│   └── record/                  # 考勤打卡记录
├── source/                      # 源码与开发调试文件
│   ├── face.c                   # 主业务逻辑：页面、触摸、人脸录入/匹配、管理员功能
│   ├── camera.c / camera.h      # V4L2 摄像头采集与 RGB/BMP 转换
│   ├── beep.c                   # 蜂鸣器测试程序
│   ├── cam_probe.c              # 摄像头格式探测工具
│   └── DRMwrap.h                # DRM 封装接口声明
└── rockx-sdk-rk1808-linux/       # RockX/RKNN SDK 与模型依赖
    ├── include/
    └── lib64/
```

## 功能说明

### 1. 首页

程序启动后显示 `1.bmp` 首页界面，提供三个入口：

- **管理员模式**：查看人脸库、查看考勤记录、删除人脸或记录、进入补卡流程
- **人脸录入**：采集当前摄像头画面，输入姓名后保存到人脸库
- **考勤打卡**：采集当前人脸，与人脸库中已录入样本进行匹配

### 2. 人脸录入

录入流程位于 `source/face.c` 的 `do_enroll()`：

1. 显示 `2.bmp` 录入页面
2. 使用 V4L2 摄像头采集 RGB 图像
3. 使用 RockX 执行人脸检测、关键点与特征提取
4. 通过屏幕软键盘输入姓名
5. 将样本保存到 `face/<姓名>_face/` 目录
6. 根据结果显示 `2_success.bmp` 或 `2_fail.bmp`

保存示例：

```text
app-runtime/face/zhangsan_face/zhangsan_001.bmp
app-runtime/face/zhangsan_face/zhangsan_002.bmp
```

### 3. 考勤打卡

打卡流程位于 `source/face.c` 的 `do_check()`：

1. 显示 `3.bmp` 打卡页面
2. 实时采集摄像头画面并绘制人脸框
3. 截取当前人脸并提取 RockX 人脸特征
4. 遍历 `face/*_face/*.bmp` 中的已录入样本
5. 计算相似度并判断是否匹配成功
6. 成功后显示 `3_success.bmp`，并将记录写入 `record/`
7. 失败后显示 `3_fail.bmp`

记录文件命名示例：

```text
app-runtime/record/zhangsan_202606031451.bmp
app-runtime/record/zhangsan_202606031451_01.bmp
```

同一分钟内重复打卡时，程序会自动追加编号，避免覆盖已有记录。

### 4. 管理员模式

管理员页面提供：

- 查看已录入人脸数量
- 查看考勤记录数量
- 分页查看人脸库与记录列表
- 删除指定人脸文件夹
- 删除指定考勤记录
- 进入补卡模式
- 返回首页

补卡模式复用普通打卡流程，但从补卡页面返回时会回到管理员页面。

## 运行环境

设备端需要具备以下资源：

- RK1808 Linux 系统
- 可用 DRM 显示设备：默认 `/dev/dri/card0`
- 可用 V4L2 摄像头：当前调试工具中使用 `/dev/video6`，实际设备可根据板卡调整
- 可用触摸输入设备
- RockX/RKNN 运行库与模型数据
- `app-runtime` 目录中的 BMP 界面资源和动态库

建议在设备端保持如下运行目录：

```text
/class_work
```

如果部署到其他路径，需要确认程序中使用的相对路径仍然正确，或者同步调整源码中的资源路径。

## 编译说明

仓库已包含设备端可执行文件。如果需要重新编译，可在 RK1808 设备或对应交叉编译环境中执行类似命令：

```bash
gcc source/face.c source/camera.c \
  -o app-runtime/f1 \
  -Irockx-sdk-rk1808-linux/include \
  -Lrockx-sdk-rk1808-linux/lib64 \
  -Lsource \
  -lrockx -lrknn_api -lDRMwrap -lpthread -ldl
```

辅助工具可按需编译：

```bash
gcc source/beep.c -o source/beep
gcc source/cam_probe.c -o source/cam_probe
```

如果在设备端运行时找不到动态库，可临时设置：

```bash
export LD_LIBRARY_PATH=$PWD:$PWD/../rockx-sdk-rk1808-linux/lib64:$LD_LIBRARY_PATH
```

或将相关 `.so` 文件复制到系统库目录。

## 部署与运行

推荐将运行目录复制到设备端，并在该目录下启动：

```bash
cd /class_work
chmod +x f1
export LD_LIBRARY_PATH=$PWD:$LD_LIBRARY_PATH
./f1
```

首次运行前请确认：

- `face/` 与 `record/` 目录存在并可写
- 摄像头、屏幕、触摸设备权限可用
- RockX 模型数据和动态库路径正确
- BMP 界面资源与程序处于同一运行目录

## 主要源码文件

- `source/face.c`：系统主程序，包含 DRM 绘制、触摸区域判断、页面切换、人脸录入、打卡匹配、管理员列表与删除逻辑
- `source/camera.c`：摄像头初始化、帧采集、RGB 数据获取、BMP 保存
- `source/camera.h`：摄像头接口声明
- `source/DRMwrap.h`：DRM 显示封装接口声明
- `source/beep.c`：蜂鸣器控制示例
- `source/cam_probe.c`：摄像头设备格式探测示例

## 数据目录约定

### 人脸库

```text
face/<姓名>_face/<姓名>_序号.bmp
```

示例：

```text
face/fqr_face/fqr_001.bmp
face/fqr_face/fqr_002.bmp
```

### 考勤记录

```text
record/<姓名>_YYYYMMDDHHMM.bmp
```

示例：

```text
record/fqr_201708051030.bmp
```

## 注意事项

- 本项目依赖 RK1808 设备端硬件环境，普通 PC 上通常只能查看源码，不能直接完整运行。
- `source/face.c` 中部分中文注释来自历史编码环境，若显示异常，不影响 C 代码逻辑编译。
- 人脸匹配阈值在 `source/face.c` 中由 `FACE_MATCH_THRESHOLD` 控制，可根据现场误识率/拒识率调试。
- UI 按钮区域通过源码中的坐标宏定义控制，修改 BMP 界面后需要同步调整触摸坐标。
- 考勤图片与人脸样本可能包含个人生物识别信息，公开使用前请确认已经获得授权。

## 后续优化方向

- 增加 Makefile 或 CMake，统一主程序和辅助工具的编译流程
- 将摄像头设备号、触摸设备、匹配阈值改为配置文件
- 增加管理员密码或权限校验
- 将考勤记录导出为 CSV/SQLite 数据库
- 对人脸样本和考勤记录增加脱敏或加密存储
