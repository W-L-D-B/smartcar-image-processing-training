# C 灰度阈值与边界实验

本目录提供可在 PC 编译运行的 C99 教学程序。默认输入由程序确定性生成：单通道 GRAY8 透视道路、明暗渐变、阴影、局部高光和固定噪点。课程以大津法为中心；其它阈值方法用于同帧比较。程序不依赖相机 SDK 或 OpenCV。

## 编译与运行

在本目录运行：

~~~powershell
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic image_lab.c -lm -o image_lab.exe
.\image_lab.exe generated
~~~

程序在 generated/ 写入 15 张单通道 PGM：

| 文件 | 输出内容 |
|---|---|
| 01_gray.pgm | 原始 GRAY8 帧 |
| 02_fixed.pgm | 固定 T=128 的二值掩码 |
| 03_otsu.pgm | 大津法阈值掩码 |
| 04_intermeans.pgm | 迭代双均值掩码 |
| 05_per_row.pgm | 逐行阈值掩码 |
| 06_local_mean.pgm | 局部均值阈值掩码 |
| 07_median.pgm | 3×3 中值去噪 |
| 08_morph_open.pgm | 开运算结果 |
| 09_morph_close.pgm | 闭运算结果 |
| 10_sobel_edges.pgm | Sobel 梯度阈值后的 0/255 边缘 |
| 11_canny_edges.pgm | 教学版 Canny 边缘 |
| 12_scan.pgm | 二值扫线输入 |
| 13_trace8.pgm | 八邻域边界爬行路径 |
| 14_ipm.pgm | 四点单应矩阵反向映射 |
| 15_downsample.pgm | 二倍最近邻降采样 |

程序还会打印 Otsu 阈值、固定阈值、灰度缓冲大小、扫描边界和八邻域步数。PGM 是 Netpbm 灰度格式，可用常见图片查看器打开；pipeline.png 给出 15 个阶段的总览。

## 函数索引

| 函数 | 主题 | 观察点 |
|---|---|---|
| make_synthetic_frame | 合成灰度输入 | 路面亮度随距离变化，阴影/高光改变直方图 |
| otsu_threshold | 大津法 | 统计 256 桶，搜索最大类间方差，返回第一档亮类灰度 T |
| threshold_fixed | 固定阈值 | gray>=T 写白色前景，否则写黑色背景 |
| intermeans_threshold / threshold_per_row / threshold_local_mean | 阈值横向比较 | 看全局、逐行和局部阈值的掩码差异 |
| median_3x3 / morphology_open / morphology_close | 去噪与掩码修复 | 孤点、细线和小洞之间的取舍 |
| sobel / sobel_threshold | Sobel 阈值边缘 | 内部计算梯度幅值；PGM 只保存二值边缘图 |
| canny_simple | Canny 教学实现 | 平滑、Sobel、非极大值抑制、双阈值连接 |
| scan_each_row / scan_center_inherited / scan_longest_column | 扫线 | 行号、左右边界、种子和有效标志 |
| trace_8_neighbor | 八邻域 | 沿边界像素逐步爬行，设置访问标记和最大步数 |
| homography_from_quad / warp_ipm | IPM | 四对地面点求 H，再用 H 的逆矩阵逐目标像素取样 |

所有图像访问使用 image[y][x]；宽 W 是列数、高 H 是行数。前景极性为白=255。80×48 尺寸用于看数组，不是总钻风相机推荐分辨率。

## 小练习

像素集合 {20,22,30,34,200,210,218,220} 用 T=35 分成两类：暗类 n0=4、均值 26.5；亮类 n1=4、均值 212。两类类间方差约 8,602.56。若多个 T 产生并列最大分数，本程序按从小到大扫描并保留先遇到的值。改为单峰数据，再观察 Otsu 为什么仍返回阈值但掩码未必对应道路。

## 移植边界

程序仅处理已获取的一帧 GRAY8 数据。接入实验室总钻风时，需要按实际 SDK 适配帧缓冲、像素布局、行跨度和缓冲区稳定时段。阈值、ROI、IPM 四点和 MCU 时间/RAM 都须用实验室图像和目标芯片复测。

