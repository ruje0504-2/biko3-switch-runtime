# 结局画面剩余差异：视口原点隔离

## 2026-10-01 当前结论

在 `3e2a0f5` 源码上，只复查之前未通过的 mode2 / 1280×720 配对：各326场景帧，2,073,600个捕获像素中77个RGB像素变化，仅1个未满足原诊断边界，位于capture1的(445,288)，RGB最大分量差9；全部画面最大RGB差49。UI像素、镜头/投影、演员矩阵和绘制队列完全一致。将两次绘制的视口原点设为0后，保持各自帧缓冲尺寸，完整RGB差异为0。

查阅 [Vulkan 不变性规范](https://docs.vulkan.org/spec/latest/appendices/invariance.html)，其中明确说明：OpenGL 的整数窗口平移不变性规则不适用于 Vulkan。[视口变换规范](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html)也说明光栅坐标精度有限。因此，这个跨视口原点的诊断差异不足以判定场景未实现或渲染算法错误；具体驱动内部原因仍未确定。

保留原阈值和 `DIFFERENCE` 结果，不将其改写为通过；也不再仅凭这项额外的主机一致性要求，把静态场景功能标为“未完成”。本次没有修改生产渲染器、4:3布局或NRO，没有扩大到其余剧情审核。旧12组汇总未重跑，不能改称当前12/12通过。当前机器记录见 `ending-raster-origin-current.json`；日志在 `local/port-closeout/`。

下方保留2026-09-29的实验记录与当时判断。


日期：2026-09-29。机器可读结果：`ending-raster-origin-verification.json`。

生产布局和渲染器未修改，画幅仍为 4:3。生产路径仍只有 9/12 完整三维配对通过，不能将以下诊断描述为画面修复或 Switch/Windows 对照通过。

## 比较器和实验

`tools/ending_viewport_probe.c` 原比较器在首个不匹配像素提前返回，导致最大误差和数量可能不完整。本次改为统计全部捕获画面，保留原 RGB 差异上限 4/255、双向 1 像素邻域与总变化面积 0.05% 边界，没有放宽阈值。默认退出码仍代表 UI 检查，完整三维结果必须另读 `Full-scene optical diagnostic`。

新增两个显式诊断参数，均只作用于探针：

- `--repeat`：两次在同一内容大小、同一原点创建场景，检查初始化、输入和帧状态是否可重复。
- `--zero-origin`：保持各自帧缓冲尺寸及原逻辑镜头/输入，把单个普通绘制视口的 GPU 原点均设为 0。探针按移动后的区域裁剪并检查外围颜色，遇到特殊副视图即拒绝；这不是可交付布局，也不覆盖双视图。

## 结果

测试后端为日志记录的 Apple M4 / MoltenVK 1.4.2。

| 实验 | 结果 |
| --- | --- |
| 同尺寸重复，story 960×720 | 326 帧；3 次完整画面捕获 RGB 差异为 0 |
| 居中至 1280×720、内容仍 960×720 | 2,073,600 个捕获像素中 77 个 RGB 像素变化，1 个不满足现有匹配边界，完整统计最大 RGB 差异 49/255 |
| 原点归零、保留不同帧缓冲尺寸 | 主机和 ASan/UBSan 各 12/12 配对、3,368 帧；全部捕获画面 RGB 差异为 0 |
| 再次运行生产布局 | UI 差异 0；所有镜头、角色 local/world/parent 矩阵和队列拓扑 12/12 一致；完整 RGB 仍 9/12，通过边界外的最大误差 131/255 |

原点实验还分别通过 3,392 个 GPU 批次检查、136 次弹窗视频帧变化、24 次 phase7 空回读。Sanitizer 测试关闭 LSan。日志中另有包含 alpha 的原始通道统计，不与显示 RGB 的上述数值混用。

当前证据说明：在此主机后端中，改变整数视口原点会触发剩余差异；只改变帧缓冲尺寸并使用相同原点，则没有差异。底层覆盖、插值、深度或采样的具体原因尚未确定，不能直接定性为驱动故障。没有通过扩大容差、改变生产构图或加入全局离屏拷贝来让检查变绿。

## 有限复现

```sh
build/ending-viewport-probe local/game/MAINDIR/Data 2 1 --repeat
build/ending-viewport-probe local/game/MAINDIR/Data 2 1
build/ending-viewport-probe local/game/MAINDIR/Data --zero-origin
ASAN_OPTIONS=detect_leaks=0 build/asan-static/ending-viewport-probe \
  local/game/MAINDIR/Data --zero-origin
```

对应日志为 `local/ending-raster-repeat-story720.log`、`ending-raster-shift-story720.log`、`ending-raster-zero-origin-host.log`、`ending-raster-zero-origin-asan.log`、`ending-raster-production-host.log`。仍未通过的是 1920×1080 mode0、1280×720 mode2 和 1920×1080 mode2；这里的 mode 是探针资源/故事配置，不是运行时的双视图选择字段。
