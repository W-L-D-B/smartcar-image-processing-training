# 合成图像处理示例

本目录的代码和图像均原创生成，不需要实验室赛道帧、摄像头或私有仓库。程序使用固定随机种子，输出用于讲解的 160×120 合成场景和几类可控失败帧。

## Windows PowerShell

~~~powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python demo.py --out generated
~~~

若当前 PowerShell 限制激活脚本，可直接运行 `.venv\Scripts\python.exe demo.py --out generated`。Python 3.10–3.12 均可使用；依赖版本锁在 requirements.txt。

## 会生成什么

- `normal_gray.png`：合成灰度输入。
- `normal_binary_fixed.png`、`normal_binary_otsu.png`、`normal_binary_adaptive.png`：三种阈值输出，白色像素表示暗前景。
- `normal_blur_otsu.png`、`normal_median.png`、`normal_morph_close.png`：滤波和形态学结果。
- `normal_edges_canny.png`：Canny 边缘候选。
- `normal_rows.png`：固定阈值结果上的逐行左右扫描和中点。
- `normal_pipeline.png`：同一输入的 3×3 中间结果对照。
- `scenario_*.png`：阴影、反光、噪点、单边缺失、急弯和低分辨率场景的输入与二值化结果。
- `metrics.csv`：输入大小、实际本机处理时间、边线有效行比例及相对合成真值的中线误差。此 CSV 仅描述运行这份合成程序的那台 PC，不是 MCU 性能报告。

程序在 CSV 中写明固定阈值、OpenCV 版本、图像尺寸和像素格式。若更改尺寸或阈值，重新运行后保留旧结果或复制输出目录，避免覆盖后失去比较基线。

## 复现实验建议

1. 先只看 `normal_gray.png`，写下你预期的边线位置。
2. 对照 fixed、Otsu 和 adaptive 的输出，记录二值前景极性。
3. 查看 `scenario_shadow` 和 `scenario_glare`，找出最早失效的处理阶段。
4. 改 `--threshold` 后重新运行，比较有效扫描行与合成真值误差。
5. 将本机算法移植到板卡时单独测 MCU；禁止把 `metrics.csv` 的 PC 时间当成 MCU 时间。
