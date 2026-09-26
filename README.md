# 图像处理

这是面向智能车实验室新生的镜头车图像处理课程。教材从总钻风摄像头输出的单通道 GRAY8 数组开始，用 C99 讲像素、灰度差/差比和、二值阈值、降采样、滤波、Sobel、扫线、八邻域和 IPM。课程不包含颜色处理或车辆控制算法。

## 文件

- site/：可直接部署的静态交互网页；章节有基础/进阶/横向扩展切换。
- handbook.md、handbook.pdf：完整中文讲义与同内容 PDF。
- course-package.zip：可直接转发的课程资料包，保留 Markdown 所需图片路径并附 PDF、C 代码、课表、来源表和许可说明。
- syllabus.md：六次课教案，共 330 分钟。
- examples/c/image_lab.c：纯 C99 灰度仿真程序，不依赖私有摄像头 SDK。
- examples/c/generated/：编译运行后生成的 19 张单通道 PGM 和总览图。
- figures/：自绘教学 SVG；figures/references/ 有三张按 CC BY-SA 4.0 署名的原文图。
- sources.csv、scope.md、CONTENT-LICENSE.md、qa-report.md：来源证据、课程边界、许可和验收。

## 编译并运行 C 实验

在 examples/c 目录执行：

~~~powershell
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic image_lab.c -lm -o image_lab.exe
.\image_lab.exe generated
~~~

默认生成确定性合成的 80×48 GRAY8 灰度帧，包括亮度渐变、阴影、局部反光和可复现噪点。程序生成 Otsu/固定/逐行/局部阈值、差比和、中值与形态学、Sobel/Canny、逐行扫线、最长白列、八邻域及 IPM 中间结果。合成结果可以复现，但不能代表实验室实拍画面、目标 MCU 速度或车辆成绩。

接入实验室相机时，仅将读取帧缓冲的适配层替换为本实验室实际 SDK；先检查灰度格式、宽高、行跨度和采集缓冲所有权。不要直接复制历史博客中的相机宏、阈值或运行参数。

## 本地查看网站

~~~powershell
python -m http.server 8000 --directory site
~~~

打开 http://127.0.0.1:8000/ 。公网网页：[图像处理](https://w-l-d-b.github.io/smartcar-image-processing-training/)。

## 来源与许可

三篇 Joshua.Xu 第18届四轮车图像系列文章声明 CC BY-SA 4.0。本培训只将三张原作者示意图按原样保留水印并署名；正文/公式说明、自绘图和 C 仿真另行编写。差比和补充页未核实授权，因此只链接并独立推导。原创 C 代码按 MIT 许可。仓库清单和证据边界见 sources.csv、figures/references/ATTRIBUTIONS.md 与 CONTENT-LICENSE.md。

本课程按实验室提供的“总钻风灰度帧”背景讲解，不推断具体型号、分辨率、主控、目标组别或最新赛规。开课前由负责人核对实际设备和当年正式规则。

