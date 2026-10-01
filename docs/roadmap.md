# 当前路线与收尾状态

更新：2026-10-01。当前使用指定 Mesa NVK，保留日文基础数据并支持汉化与去码外挂。主要游戏功能已经接入生产应用；当前工作是修复实际问题和确认最新 Switch 构建，不能再沿用早期“M2/M3/M4未开始”的状态。

| 阶段 | 当前实现 | 主要证据与范围 |
| --- | --- | --- |
| M0 工程与平台 | 模块边界、主循环、libnx输入/音频、资源服务、NRO构建已接通；Mesa锁定 `5dba7886…` | [架构](architecture.md)、[NVK依赖锁](../config/dependencies.lock.json)、最新构建清单 |
| M1 静态画面与镜头 | 实际背景、灯光/雾、投影、4:3视口、深度及透明层已接入 | [光影修复](../reports/lighting-surface-layers.md)；用户确认表面明暗闪烁已修复。跨视口原点的微小主机差异见[单独诊断](../reports/ending-raster-origin.md)，不是缺失场景 |
| M2 动态角色 | ANIM/XAN、ENVL、MORP、头/眼节点、材质动画和CPU/GPU姿态发布已接入 | [演员姿态](../reports/actor-pose.md)、[GPU蒙皮](../reports/gpu-skin-performance.md)、[实际NEON对照](../reports/neon-skinning.md) |
| M3 世界交互 | 玩家/NPC移动、追踪、镜头、地面/墙面/道具碰撞和天气已接入 | [实际游戏入口](../reports/play-session.md)、[藏身退出修复](../reports/hiding-exit.md)、[NPC视线优化](../reports/npc-occlusion-performance.md)；雨天及撞车曾获用户实机确认 |
| M4 游戏与菜单 | 标题、选人、剧情、游戏、失败/重试、暂停、结局/回放、特殊场景、鉴赏、音量及相册均有生产入口 | [原标题入口](../reports/original-front-end.md)、[结局回放](../reports/ending-gallery-session.md)、[特殊场景](../reports/special-session.md)、[相册](../reports/album-application.md)、[菜单操作](../reports/menu-shortcuts.md) |
| M5 音视频与时序 | BGM/语音/音效、独立音频补给、AVI纹理与游戏/媒体时钟已接入 | [原版速率](../reports/game-clock-fps.md)、[音频默认值](../reports/audio-defaults.md)、[特殊场景媒体](../reports/special-media.md)；主机输出不能当作最新Switch声音结果 |
| M6 持久化 | 存读档、下一剧情交接、结局动作记录、解锁、照片和音量配置已接入 | [保存后续剧情修复](../reports/checkpoint-next-story.md)、[动作记录存储](../reports/record-storage.md)、[音量应用](../reports/volume-application.md)。中文资源与说明已按本次要求接入可移除的[合并外挂](../reports/combined-patch.md) |
| M7 收尾 | 主机原版对照、针对性ASan/UBSan与NVK交叉构建已有结果；最新NRO的实机性能/稳定性仍缺结果 | 每次仅验证本次改动所需范围，不把不同日期的测试累加成同一二进制的整体验收 |

## 当前构建

`build-switch/biko3-preview.nro`，版本字段0.3.5，16,343,096字节；SHA256 `aa2f9d1ea034719bd8d796efcc38d6b963f7659bb28a804dc9f182266f8209bb`，ELF未解析符号0。

包含可选汉化/去码外挂、关闭 Switch 日志生成，以及[渲染绑定缓存](../reports/vulkan-bindings.md)、[藏身退出修复](../reports/hiding-exit.md)和[NPC无效遮挡检查优化](../reports/npc-occlusion-performance.md)。计数下降只代表相应测试路径的工作量减少，不能当作实机FPS提升。

## 接下来的边界

1. 用户已试玩并明确停止逐段剧情审核；其他藏身点由用户自行检查。不再自动扩展自然路线矩阵，也不将未跑完整路线写成通过。
2. 后续优先处理实际反馈或日志能定位的问题；已有检查通过后，仅因新改动、失败或未解决疑点补必要验证。
3. 最新NVK实机运行、1GHz CPU下的帧率/帧时间和持续运行结果尚无新证据。本机MoltenVK不能替代它们。
4. 此前一次 GitHub 推送已完成；本批仅本地提交与增量交付，不追加推送、不重复整包。Mac防休眠保持，整个移植结束后恢复。

历史实现细节、原指令地址和当时未完成项保留在 `reports/` 与Git历史中；阅读旧报告时以其日期和本页当前状态为准。完整移植的最终验收仍未宣告完成。
