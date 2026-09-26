# 镜头车图像处理新生培训

中文教学资料与可公开浏览的静态课程站。全部示意图为原创 SVG，示例帧由 `examples/demo.py` 生成；引用的队伍仓库和个人资料只作出处链接/摘要，未复制其代码、截图、视频或界面。

## 本地预览网站

从本目录运行：

~~~powershell
python -m http.server 8000 --directory site
~~~

浏览器打开 `http://127.0.0.1:8000/`。网站不依赖第三方 CDN 或登录。

## 培训讲义与示例

- `handbook.md`：可直接转发的完整讲义。
- `handbook.pdf`：同内容 PDF，A4 14 页，已逐页渲染检查。
- `syllabus.md`：6 次课、320 分钟的安排。
- `sources.csv`：来源、证据位置、访问日期和使用限制。
- `scope.md`：赛规范围、资料层级与未核验项。
- qa-report.md：代码、PDF、网站和公开访问验收证据。
- `figures/`：9 张可编辑原创 SVG。
- `examples/`：Python/OpenCV 代码和可复现合成帧。

示例代码：

~~~powershell
cd examples
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python demo.py --out generated
~~~

程序生成的 `metrics.csv` 只记录运行机器的本机测量，不是 MCU 性能报告。

## 公开网页发布

`site/` 是完整静态页面，也包括讲义、来源表和示例压缩包。仓库的 GitHub Actions 工作流会把 `site/` 发布到 GitHub Pages。首次创建站点时若 GitHub 要求选择发布源，在仓库 Settings → Pages 选择 GitHub Actions。公开访问 URL 只有在推送后通过未登录会话检查，才会在 QA 报告和交付说明中写为“已公开”。

## 课程适用边界

课程按通用单目摄像头循迹设计。实验室目标届次、组别、相机与主控未提供；开课前要按组委会最终规则核对传感器、限高、任务元素和硬件。KDocs 版式参考本次访问失败，网页按执行流程给出的备用结构制作。
