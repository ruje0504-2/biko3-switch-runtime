# 普通结局生产场景 UI 接入

2026-09-28。本阶段把已经恢复的结局 UI 组合器接入 `ending_normal_session` 的真实生产场景。范围仍是 `4cf318`/`4d00fa` 两个普通结局加载器；其它加载器、完整 flow16 子阶段和结局特殊控制器仍按显式失败处理。

## 实现

- 普通结局场景现在拥有真实的 `ending_ui_render`、`curtain_render`、`ending_ui_batch`、`ending_stage_ui` 和公共 HUD 状态，资源由场景生命周期创建并在 CPU/渲染器资源释放前按依赖顺序销毁。`4d00fa` 会选择 `BK_ENDING_UI_SECONDARY` 并创建阶段 UI 的 GPU 所有者，覆盖其 63..69 槽位；`4cf318` 仍使用普通 UI 所有者。
- 每帧先开启 UI 批次，再执行结局状态、角色姿态、BOM、相机和普通 GPU 队列；所有状态成功后，使用真实主角色节点 world、真实相机视图、原版 FOV/裁剪面和当前 renderer viewport 构造 UI 绑定，最后准备 UI 网格。
- UI 拾取使用已发布的 39 个主角色节点和实际视图/投影矩阵。工具栏、提示、选择、游标、黑幕、尾部提示和六个已拥有的结局音频槽均经过真实场景回调；音频状态从当前结局音频所有者读取。
- 资源重载、最终图片所有权、离开场景、灯光重载和 flow 调度仍返回明确失败，避免用诊断夹具伪造完整流程。未拥有的音频槽也明确报告不可用。
- `ending-normal-scene-probe` 现在挂载 `bk3_00` UI 资源，故事入口和鉴赏入口都走同一生产 UI 所有者。

## 验证

- 故事入口：`ending normal scene PASS entry=story variant=0 frames=64 draws=10725 skin=520 audio_frames=62400 allocations=624`。
- 鉴赏入口 variant 0：同样 10,725 个绘制项、520 次蒙皮和 62,400 音频帧；variant 1：6,695 个绘制项、520 次蒙皮、62,400 音频帧和 774 次 GPU 分配，阶段图片已进入生产资源生命周期。
- `build/asan` 全量 CTest：92/92 通过；主机 `test-host.sh local/game/MAINDIR` 全量检查通过，包含原标题→选人→雨天→游戏→拍照→暂停/恢复→返回标题、存档恢复和撞车失败重试路径。
- `./build-switch.sh` 通过，当前源码 NRO SHA-256：`d0cef8772758f0486233679fdd044ce784621f1c79ddbf0825e27afa30b18562`。这是指定 Mesa/NVK 交叉构建结果；尚不等于 Switch 实机结局验证。

ASan 使用 `detect_leaks=0`。主机 MoltenVK 检查和 Switch 交叉构建不能替代 Switch 实机性能/结局验收。Mac 防休眠保持，完整移植尚未结束。

## 后续边界

下一步继续接入结局其它加载器和 phase 8/9 控制器；当前 `48302b`、`48bcbb`、`4e2223` 以及 `4d1025`、`4d2320`、`4d39e6` 路径仍会失败并回收已加载资源。交付目录不生成 ZIP，原始素材目录继续只读。
