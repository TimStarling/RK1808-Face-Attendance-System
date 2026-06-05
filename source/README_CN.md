# 人脸考勤识别系统说明

本文档对应当前源码版本：

- 主程序：`face.c`
- 摄像头封装：`camera.c`、`camera.h`
- 运行目录：`/class_work`
- 主可执行文件：`/class_work/f1`

说明：本文中的行号是当前版本源码中的函数起始行号，后续继续修改源码后行号可能变化。

## 一、主要功能

### 1. 首页

程序启动后显示 `1.bmp` 主界面。

首页提供三个入口：

- 管理员模式
- 人脸录入
- 考勤打卡

触摸区域在 `face.c` 顶部宏定义中配置：

- 管理员模式：`BTN_ADMIN_X1 ~ BTN_ADMIN_Y2`
- 人脸录入：`BTN_ENROLL_X1 ~ BTN_ENROLL_Y2`
- 考勤打卡：`BTN_CHECK_X1 ~ BTN_CHECK_Y2`

### 2. 人脸录入

进入录入界面后显示 `2.bmp`，并在指定区域显示摄像头实时画面。

当前版本已经优化为：

```text
摄像头采集 -> RGB 内存 -> RockX 检测 -> DRM 显示 -> 直接画人脸框
```

点击录入按钮后：

1. 将当前 RGB 画面保存为 `./face/enroll_preview.bmp`
2. 从当前 RGB 内存中提取人脸特征
3. 弹出 26 键英文键盘输入姓名
4. 以姓名创建目录，例如 `fqr_face`
5. 将图片保存为 `fqr_001.bmp`、`fqr_002.bmp`
6. 显示录入成功或失败页面

保存示例：

```bash
/class_work/face/fqr_face/fqr_001.bmp
/class_work/face/fqr_face/fqr_002.bmp
```

### 3. 考勤打卡

进入打卡界面后显示 `3.bmp`，并显示摄像头实时画面和人脸框。

点击打卡按钮后：

1. 保存当前 RGB 画面为 `./face/check_preview.bmp`
2. 从当前 RGB 内存提取打卡人脸特征
3. 遍历 `./face/*_face/*.bmp`
4. 与所有已录入人脸做相似度匹配
5. 匹配成功显示 `3_success.bmp`
6. 顶部显示 `Hi,姓名`
7. 将本次打卡图片保存到 `record` 目录

打卡记录保存示例：

```bash
/class_work/record/fqr_202606031451.bmp
```

如果同一分钟重复打卡，为避免覆盖，会追加编号：

```bash
fqr_202606031451_01.bmp
```

### 4. 管理员模式

管理员模式从首页左上角进入。

已实现功能：

- 查看人脸库数量
- 查看打卡记录数量
- 查看所有人脸文件夹
- 查看所有打卡记录
- 删除指定人脸文件夹
- 删除指定打卡记录
- 补卡
- 返回首页

管理员界面使用程序内置中文点阵字库显示中文，不依赖系统字体。

### 5. 补卡功能

管理员界面点击“补卡”后进入考勤打卡界面。

补卡和普通考勤打卡流程完全一致：

- 实时摄像头画面
- 人脸检测框
- 人脸匹配
- 成功/失败页面
- 成功后保存到 `record` 目录

区别：

- 在补卡界面点击返回键，会返回管理员界面，而不是首页。

### 6. 文件目录结构

设备端运行目录：

```bash
/class_work
```

主要文件：

```bash
1.bmp
2.bmp
2_success.bmp
2_fail.bmp
3.bmp
3_success.bmp
3_fail.bmp
f1
libcamera.so
libDRMwrap.so
face/
record/
```

人脸库目录：

```bash
/class_work/face
```

打卡记录目录：

```bash
/class_work/record
```

## 二、主要函数说明

### 1. 程序入口和整体控制

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `main()` | 2355 | 程序入口，初始化目录、DRM、触摸、摄像头、RockX，然后进入首页循环 |
| `face.c` | `cleanup()` | 2321 | 释放摄像头、触摸、DRM、RockX 资源 |
| `face.c` | `handle_signal()` | 83 | 处理 `SIGINT/SIGTERM`，让主循环退出 |

### 2. 页面功能函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `do_enroll()` | 2192 | 人脸录入页面逻辑 |
| `face.c` | `do_check()` | 2252 | 考勤打卡页面逻辑 |
| `face.c` | `do_admin()` | 2122 | 管理员页面逻辑 |
| `face.c` | `draw_admin_page()` | 2046 | 绘制管理员页面 |
| `face.c` | `show_result_then_restore()` | 1545 | 显示成功/失败页面后恢复原页面 |
| `face.c` | `show_check_success_with_name()` | 1553 | 打卡成功时显示 `Hi,姓名` |

### 3. 摄像头和预览函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `camera.c` | `init_video()` | 157 | 初始化摄像头，自动尝试 `/dev/video6`、`/dev/video7`、`/dev/video0`、`/dev/video1` |
| `camera.c` | `get_rgb_frame()` | 287 | 从摄像头采集一帧并转换为 RGB 内存数据 |
| `camera.c` | `save_rgb_bmp()` | 282 | 将 RGB 内存帧保存为 BMP 文件 |
| `camera.c` | `get_bmp()` | 313 | 兼容旧接口：采集一帧并保存为 BMP |
| `camera.c` | `free_video()` | 323 | 释放摄像头映射和设备文件 |
| `face.c` | `update_face_preview()` | 1501 | 更新实时预览：采集 RGB、显示到 DRM、检测人脸并画框 |
| `face.c` | `rgb_draw_no_scale_region()` | 1358 | 将 RGB 内存画面直接绘制到屏幕指定区域 |
| `face.c` | `make_rgb_image()` | 1394 | 将 RGB 内存封装为 `rockx_image_t` |

### 4. 人脸识别函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `face_init()` | 1223 | 初始化 RockX 人脸检测、五点关键点、人脸识别模块 |
| `face.c` | `get_max_face()` | 1250 | 从检测结果中选出面积最大的人脸 |
| `face.c` | `extract_face_feature()` | 1272 | 从 BMP 文件中提取人脸特征，用于遍历已录入人脸库 |
| `face.c` | `extract_face_feature_from_rgb()` | 1404 | 从 RGB 内存帧中提取当前人脸特征 |
| `face.c` | `match_enrolled_faces()` | 1935 | 遍历 `face/*_face/*.bmp` 并匹配当前打卡人脸 |

### 5. 录入保存和打卡记录函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `save_enrolled_face_image()` | 1686 | 将录入图片保存到 `姓名_face/姓名_编号.bmp` |
| `face.c` | `get_next_named_face_index()` | 1641 | 获取某个人脸目录下的下一个编号 |
| `face.c` | `save_check_record_image()` | 1719 | 将打卡成功图片保存到 `record/姓名_年月日小时分钟.bmp` |
| `face.c` | `copy_file()` | 1576 | 复制文件 |
| `face.c` | `ensure_face_dir()` | 615 | 确保 `face` 目录存在 |
| `face.c` | `ensure_record_dir()` | 624 | 确保 `record` 目录存在 |

### 6. 管理员功能函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `count_face_dirs()` | 1792 | 统计人脸库中 `*_face` 文件夹数量 |
| `face.c` | `count_record_files()` | 1816 | 统计打卡记录数量 |
| `face.c` | `collect_admin_items()` | 1820 | 收集管理员界面当前页要显示的人脸库或记录 |
| `face.c` | `delete_admin_item()` | 1915 | 删除管理员界面选中的人脸库或打卡记录 |
| `face.c` | `delete_dir_recursive()` | 1873 | 递归删除人脸文件夹 |
| `face.c` | `count_bmp_files_in_dir()` | 1768 | 统计某目录下 BMP 数量 |

### 7. 触摸输入函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `init_touch()` | 647 | 自动扫描触摸设备并打开对应 `/dev/input/eventX` |
| `face.c` | `open_touch_event()` | 633 | 打开指定触摸 event |
| `face.c` | `wait_touch()` | 712 | 阻塞等待一次触摸坐标，支持 `BTN_TOUCH` 和 `SYN_REPORT` |
| `face.c` | `poll_touch()` | 890 | 非阻塞轮询触摸，用于实时预览页面 |
| `face.c` | `drain_touch_events()` | 759 | 清空触摸事件残留，减少误触 |
| `face.c` | `in_rect()` | 88 | 判断坐标是否在按钮区域内 |

### 8. 键盘和文字绘制函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `input_face_name()` | 845 | 录入成功后弹出英文键盘输入姓名 |
| `face.c` | `draw_name_keyboard()` | 766 | 绘制 26 键英文键盘 |
| `face.c` | `keyboard_key_at()` | 809 | 根据触摸坐标判断按下哪个键 |
| `face.c` | `draw_text()` | 253 | 绘制英文/数字点阵文字 |
| `face.c` | `draw_utf8_text()` | 567 | 绘制 UTF-8 中文文字 |
| `face.c` | `draw_cn_glyph()` | 550 | 绘制单个中文点阵字 |
| `face.c` | `find_cn_glyph()` | 541 | 查找中文点阵 |
| `face.c` | `utf8_next_code()` | 520 | 解析 UTF-8 字符 |

### 9. DRM/BMP 绘制函数

| 文件 | 函数 | 行号 | 作用 |
| --- | --- | ---: | --- |
| `face.c` | `draw_pixel()` | 92 | 在 DRM 显存中画一个像素 |
| `face.c` | `fill_rect()` | 105 | 填充矩形 |
| `face.c` | `draw_rect_border()` | 114 | 绘制矩形边框，人脸框也使用类似方式 |
| `face.c` | `draw_line()` | 128 | 绘制线条 |
| `face.c` | `draw_back_button()` | 157 | 绘制返回按钮 |
| `face.c` | `clear_screen_rect()` | 934 | 清空指定屏幕区域 |
| `face.c` | `bmp_show()` | 1137 | 显示整张 BMP 页面图 |
| `face.c` | `bmp_draw_no_scale_region()` | 1046 | 不缩放地绘制 BMP 到指定区域 |
| `face.c` | `bmp_draw_scaled()` | 961 | 缩放绘制 BMP，目前保留备用 |

## 三、从 `main()` 开始的程序运行流程

`main()` 位于 `face.c:2355`。

### 1. 注册退出信号

程序首先注册：

```c
signal(SIGINT, handle_signal);
signal(SIGTERM, handle_signal);
```

当用户按 `Ctrl+C` 或进程收到退出信号时，`handle_signal()` 会把 `g_running` 设置为 `0`，主循环随后退出。

### 2. 创建数据目录

程序调用：

```c
ensure_face_dir();
ensure_record_dir();
```

确保运行目录下存在：

```bash
./face
./record
```

对应设备路径通常是：

```bash
/class_work/face
/class_work/record
```

### 3. 初始化 DRM 显示

程序打开：

```bash
/dev/dri/card0
```

然后调用：

```c
DRMinit();
DRMcreateFB();
```

创建屏幕显示缓冲区，后续所有页面、文字、摄像头画面都写入 `g_drm.vaddr`，再通过 `DRMshowUp()` 刷新到屏幕。

### 4. 初始化触摸设备

程序调用：

```c
init_touch();
```

该函数会读取：

```bash
/proc/bus/input/devices
```

自动寻找带 `ABS` 坐标能力的触摸设备，并打开对应：

```bash
/dev/input/eventX
```

如果自动查找失败，会兜底尝试 `event0` 到 `event9`。

### 5. 初始化摄像头

程序调用：

```c
init_video();
```

该函数会依次尝试：

```bash
/dev/video6
/dev/video7
/dev/video0
/dev/video1
```

找到支持 `640x480 YUYV` 的摄像头后，申请 V4L2 缓冲区并启动视频流。

### 6. 初始化 RockX 模型

程序调用：

```c
face_init();
```

初始化三个模块：

- `ROCKX_MODULE_FACE_DETECTION`
- `ROCKX_MODULE_FACE_LANDMARK_5`
- `ROCKX_MODULE_FACE_RECOGNIZE`

这三个模块分别用于：

- 人脸检测
- 人脸关键点对齐
- 人脸特征提取

### 7. 进入首页循环

初始化完成后，程序进入：

```c
while (g_running)
```

每次循环先显示：

```c
bmp_show("./1.bmp", 0, 0);
```

然后等待用户触摸：

```c
wait_touch(&x, &y);
```

根据触摸坐标进入不同页面：

```c
管理员模式 -> do_admin()
人脸录入   -> do_enroll()
考勤打卡   -> do_check()
```

### 8. 进入人脸录入页面

`do_enroll()` 位于 `face.c:2192`。

流程：

1. 显示 `2.bmp`
2. 绘制返回按钮
3. 循环调用 `update_face_preview()`
4. 从摄像头直接采集 RGB 内存帧
5. 直接显示 RGB 画面
6. RockX 检测人脸
7. 在屏幕上画人脸框
8. 用户点击录入按钮
9. 保存当前 RGB 帧为 `enroll_preview.bmp`
10. 从 RGB 内存提取人脸特征
11. 弹出英文键盘输入姓名
12. 保存到 `face/姓名_face/姓名_编号.bmp`
13. 显示成功或失败页面

### 9. 进入考勤打卡页面

`do_check()` 位于 `face.c:2252`。

流程：

1. 显示 `3.bmp`
2. 绘制返回按钮
3. 循环调用 `update_face_preview()`
4. 显示实时摄像头画面和人脸框
5. 用户点击打卡按钮
6. 保存当前 RGB 帧为 `check_preview.bmp`
7. 从 RGB 内存提取当前人脸特征
8. 调用 `match_enrolled_faces()`
9. 遍历 `face/*_face/*.bmp`
10. 匹配成功后显示 `3_success.bmp`
11. 顶部显示 `Hi,姓名`
12. 调用 `save_check_record_image()` 保存打卡记录
13. 匹配失败则显示 `3_fail.bmp`

### 10. 进入管理员页面

`do_admin()` 位于 `face.c:2122`。

流程：

1. 调用 `draw_admin_page()` 绘制管理员页面
2. 显示人脸库数量
3. 显示打卡记录数量
4. 显示人脸库列表或打卡记录列表
5. 支持切换 `人脸库` 和 `打卡记录`
6. 支持上一页、下一页
7. 点击删除按钮删除对应数据
8. 点击补卡进入 `do_check()`
9. 点击返回回到首页

补卡功能直接复用 `do_check()`，因此补卡打卡流程与普通打卡一致。区别是它由管理员页面调用，返回后继续回到管理员页面。

### 11. 程序退出

当 `g_running` 变为 `0` 或初始化失败时，程序调用：

```c
cleanup();
```

释放：

- 摄像头资源
- 触摸设备
- DRM 显示资源
- RockX 模型资源

然后 `main()` 返回。

## 四、编译和运行

### 1. 编译摄像头库

```bash
cd /home/fang/Desktop/work
/opt/rk1808-sdk/buildroot/output/rockchip_rk1808/host/bin/aarch64-linux-gcc \
    -fPIC -shared camera.c -o libcamera.so
```

### 2. 编译主程序

```bash
cd /home/fang/Desktop/work
/opt/rk1808-sdk/buildroot/output/rockchip_rk1808/host/bin/aarch64-linux-gcc face.c -o f1 \
    -I../rockx-rk1808-Linux/include \
    -L../rockx-rk1808-Linux/lib64 \
    -L./ \
    -lrockx -lrknn_api -lcamera -lDRMwrap -ldrm -lpthread \
    -Wl,--enable-new-dtags -Wl,-rpath,./:/class_work
```

### 3. 推送到设备

```bash
adb push f1 /class_work/f1
adb push libcamera.so /class_work/libcamera.so
```

### 4. 运行

```bash
adb shell
cd /class_work
killall f1 2>/dev/null
LD_LIBRARY_PATH=./:/class_work:/lib ./f1
```

## 五、注意事项

1. 程序需要从 `/class_work` 目录运行，因为 BMP 图片和动态库使用相对路径。
2. `/class_work/libcamera.so` 必须是当前版本，否则 `get_rgb_frame()`、`save_rgb_bmp()` 可能找不到。
3. 如果触摸无效，先查看：

```bash
adb shell cat /proc/bus/input/devices
adb shell ls -l /dev/input
```

4. 如果摄像头无法打开，先查看：

```bash
adb shell v4l2-ctl --list-devices
adb shell ls -l /dev/video*
```

5. 当前实时预览已改为 RGB 内存直显，不再每帧生成预览 BMP。只有点击录入或打卡时才保存当前帧。
