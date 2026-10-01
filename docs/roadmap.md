# 当前路线与收尾状态

更新：2026-10-02。当前使用指定 Mesa NVK，保留日文基础数据并支持汉化与去码外挂。主要游戏功能已经接入生产应用，本次工程收尾交付 NRO、原版直装 NSP 与汉化去码更新 NSP；后续实机由用户自行验证，按反馈维护。

| 阶段 | 当前实现 | 主要证据与范围 |
| --- | --- | --- |
| M0 工程与平台 | 模块边界、主循环、libnx输入/音频、资源服务、NRO与原生NSO/NSP构建已接通；Mesa锁定 `5dba7886…` | [架构](architecture.md)、[NVK依赖锁](../config/dependencies.lock.json)、[直装包检查](../reports/nsp-1.0.78.md) |
| M1 静态画面与镜头 | 实际背景、灯光/雾、投影、4:3视口、深度及透明层已接入 | [光影修复](../reports/lighting-surface-layers.md)；用户确认表面明暗闪烁已修复。跨视口原点的微小主机差异见[单独诊断](../reports/ending-raster-origin.md)，不是缺失场景 |
| M2 动态角色 | ANIM/XAN、ENVL、MORP、头/眼节点、材质动画和CPU/GPU姿态发布已接入 | [演员姿态](../reports/actor-pose.md)、[GPU蒙皮](../reports/gpu-skin-performance.md)、[实际NEON对照](../reports/neon-skinning.md) |
| M3 世界交互 | 玩家/NPC移动、追踪、镜头、地面/墙面/道具碰撞和天气已接入 | [实际游戏入口](../reports/play-session.md)、[藏身退出修复](../reports/hiding-exit.md)、[NPC视线优化](../reports/npc-occlusion-performance.md)；雨天及撞车曾获用户实机确认 |
| M4 游戏与菜单 | 标题、选人、剧情、游戏、失败/重试、暂停、结局/回放、特殊场景、鉴赏、音量及相册均有生产入口 | [原标题入口](../reports/original-front-end.md)、[结局回放](../reports/ending-gallery-session.md)、[特殊场景](../reports/special-session.md)、[相册](../reports/album-application.md)、[菜单操作](../reports/menu-shortcuts.md) |
| M5 音视频与时序 | BGM/语音/音效、独立音频补给、AVI纹理与游戏/媒体时钟已接入 | [原版速率](../reports/game-clock-fps.md)、[音频默认值](../reports/audio-defaults.md)、[特殊场景媒体](../reports/special-media.md)；主机输出不能当作最新Switch声音结果 |
| M6 持久化 | 存读档、下一剧情交接、结局动作记录、解锁、照片和音量配置已接入；NSP按所选用户写HOS存档并显式提交 | [保存后续剧情修复](../reports/checkpoint-next-story.md)、[动作记录存储](../reports/record-storage.md)、[HOS存储与检查范围](../reports/nsp-1.0.78.md) |
| M7 收尾 | NRO/原版NSP/更新NSP与优化外挂交付；主机、ASan及NVK构建通过，实际NSP内容已提取校验 | [外挂优化](../reports/patch-loading.md)、[NSP成品检查](../reports/nsp-1.0.78-verification.json)。用户接手后续实机验证，不把主机检查当作实机结果 |

## 当前构建

`build-switch/biko3-runtime.nro`，版本字段1.0.78，16,408,236字节；SHA256 `11afda078af0f9d0479b83057d1469e708ada317ea790326552fba1327ac9fb9`，ELF未解析符号0。名称/作者为 `biko3-runtime` / `ILLUSION`，图标来自本地 `icon.jpg`。

`交付/biko3-01094F68D7330000.nsp`，名称「尾行3」、TitleID `01094F68D7330000`，2,957,146,840字节，SHA256 `85b4cf4706fd43284ab35dff0e78986334a6eb9b7ebecbbfcbf8b5978f428df6`。内含原版日文Data，不含外挂；NRO与NSP成品字段见[打包核对](../reports/nsp-1.0.78-verification.json)。

包含可选汉化/去码外挂、关闭 Switch 日志生成，以及[渲染绑定缓存](../reports/vulkan-bindings.md)、[藏身退出修复](../reports/hiding-exit.md)和[NPC无效遮挡检查优化](../reports/npc-occlusion-performance.md)。本次外挂改为37,165,253字节的逐资源压缩格式，完整替换跳过原载荷；计数下降只代表相应路径工作量减少，不能当作实机FPS或加载耗时收益。

`交付/biko3-01094F68D7330800-update.nsp` 是关联新本体的 BKTR 差分 Patch 更新，49,003,232 字节，复用全部168原版Data文件，包含程序及合并汉化/去码补丁。内容版本131072，显示版本1.0.78cn；Program和存档owner仍为本体ID。见[更新包核对](../reports/nsp-update-1.0.78-verification.json)。

最新追加[中文流程文字修复](../reports/chinese-dialogue.md)：实际字形高度取代字节估算，消除短对白多消耗一次 A 的情况；中文对白、世界开场/失败/道具提示放大30%、按完整行裁切，日文、菜单及按键说明不变。共用文字纹理改为复用。121段文本242裁切、中文UI及中日文字/流程提示普通与ASan配对通过，未扩大剧情审核。

## 接下来的边界

1. 用户已试玩并明确停止逐段剧情审核；其他藏身点由用户自行检查。不再自动扩展自然路线矩阵，也不将未跑完整路线写成通过。
2. 后续优先处理实际反馈或日志能定位的问题；已有检查通过后，仅因新改动、失败或未解决疑点补必要验证。
3. 用户明确“余后我自己验证”，最新NSP安装、HOS照片持久性、NVK性能与持续运行由用户接手。本机MoltenVK不能替代实机，不再把这些缺少的新结果作为交付阻塞。
4. 此前一次 GitHub 推送已完成；本批仅本地提交，不追加推送。按本次请求制作原版NSP与补丁更新NSP，既有NRO外挂目录只做增量更新，不生成ZIP或另一个完整SD包。工程交付结束后恢复Mac正常休眠。

历史实现细节、原指令地址和当时未完成项保留在 `reports/` 与Git历史中；阅读旧报告时以其日期和本页当前状态为准。工程实现与本次交付完成，不将未跑的全剧情/实机范围写成通过。
