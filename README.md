# 图像处理

面向智能车实验室新生的镜头车灰度图像课程。教材从总钻风摄像头单通道 GRAY8 数组开始，以大津法为重点讲直方图、阈值选择和二值结果，再用 C99 讲降采样、滤波、Sobel 阈值边缘、扫线、八邻域和 IPM。本课不包含颜色处理或车辆控制算法。

## 文件

- site/：可直接部署的静态交互网页；有 6 个互动实验和 8 个图像处理章节。
- handbook.md、handbook.pdf：完整中文讲义与同内容 PDF。
- course-package.zip：可直接转发的课程包，包含 MD/PDF、插图、课表、来源、许可和 C 示例。
- syllabus.md：六次课教学安排，共 330 分钟。
- examples/c/image_lab.c：不依赖私有摄像头 SDK 的 C99 灰度仿真程序。
- examples/c/generated/：编译运行后生成 15 张单通道 PGM 和一张总览图。
- figures/：自绘教学 SVG；figures/references/ 有 3 张按 CC BY-SA 4.0 署名的原文图。
- sources.csv、CONTENT-LICENSE.md、qa-report.md：来源证据、许可和验收记录。

## 编译并运行 C 实验

在 examples/c 目录执行：

~~~powershell
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic image_lab.c -lm -o image_lab.exe
.\image_lab.exe generated
~~~

程序固定生成一张 80×48 GRAY8 合成帧，包含亮度渐变、阴影、局部高光和可复现噪点，并输出固定阈值、大津、迭代/逐行/局部阈值、中值/形态学、Sobel 阈值边缘、Canny、扫线、八邻域和 IPM 中间图。结果可复现，但不代表实验室实拍帧、目标 MCU 速度或车辆成绩。

接入实验室相机时，仅把帧读取层替换为实验室实际 SDK，并检查灰度格式、宽高、行跨度和帧缓冲所有权。大津阈值与标定点都要重新根据授权图像验证。

## 本地查看网页

~~~powershell
python -m http.server 8000 --directory site
~~~

打开 http://127.0.0.1:8000/ 。公网网页：[图像处理](https://w-l-d-b.github.io/smartcar-image-processing-training/)。

## 推荐参考与许可

- 主参考为 Joshua.Xu [第18届系列目录](https://blog.csdn.net/qq_58114029/article/details/131879413)及[图像处理篇](https://blog.csdn.net/qq_58114029/article/details/132050763)、[边线提取篇](https://blog.csdn.net/qq_58114029/article/details/132132607)。文章声明 CC BY-SA 4.0；课程仅原样采用三张带水印的示意图并署名，文字和 C 示例为原创。
- Otsu 算法定义参考 [OpenCV 阈值文档](https://docs.opencv.org/4.12.0/d7/d4d/tutorial_py_thresholding.html)；其余仓库授权边界见 sources.csv。
- 原创 C99 代码按 MIT 许可；教学文字和原创图按 CC BY-SA 4.0。详见 CONTENT-LICENSE.md 和 figures/references/ATTRIBUTIONS.md。

案例来自第18届历史图文，不能直接作为当前组别的参数。开课前用实验室真实相机帧确认分辨率、阈值和四点标定。

