# game 模块：入口/镜头策略依赖已实现，完整 M4 未完成

为 M1 实际镜头恢复提供 `entry` 与 `camera_policy`，这一部分恢复入口选择和镜头分派：45组路线/演员/头节点/玩家起点，进入前流程对 phase 的覆盖，特定流程的路线游标覆盖，mode-2 镜头选择/初始位置及两种镜头交接的结束条件。路线游标由调用方明确传入，不猜存档或全局初始化内容。

依赖 core/world/resource，不依赖 Vulkan 或 libnx。scene 的 `entry_assets` 负责资源读取和所有权，不由 game 打开 PP、创建 GPU 或自动启动任务。phase=1 玩家九镜头已由独立player_view入口接通，需要明确的玩家/控制上下文。phase=3 等原分支保持镜头，是经原代码验证的无更新，不表示玩法完成。

NPC追踪AI已作为CPU组件恢复；完整的开始/暂停/结束、任务触发、对话、关卡状态与应用组合仍待 M4。将来消费固定步输入与 world 查询，产生世界命令、场景切换请求、媒体请求和可序列化状态。任务结果必须有真实状态依据，不能把选择元数据或诊断姿态当作游戏运行。

后续恢复NPC移动依赖：`npc_motion`解析动作优先级/计时，`npc_point`解析到点动作并输出待消费媒体命令，`npc_route`按原顺序串联移动、最后跨点和吸附，`npc_action`只恢复AI尾部的动作结束切换。不能无条件调用尾部替代完整AI，不能忽略媒体命令后宣称完整游戏更新。`player_animation`在已确定动作与物理位置后提交主玩家姿态：idle .2倍时间，其余 .5倍；0/4/10默认动作使用固定10 tick过渡，其他使用当前配置过渡。输入、动作选择、附件、材质和音效仍由后续调用方恢复。见 `reports/movement-animation.md`。

最新玩家control/view/presentation与动作音频组合见`reports/player-presentation.md`，替代上文早期动画阶段尚缺玩家输入/附件/材质/音效的历史状态。动态道具、背景及完整任务驱动仍未完成。

背景`background_config/background`恢复原4f737b及资源分派；game只输出有序音频/动画命令，真实PCM/模型/ATR由scene拥有。计时器、随机数和环境音边沿跨帧保留。验证见`reports/background-lifecycle.md`，原游戏主流程及天气绘制仍待接入。

`lighting_pass` 恢复原灯组、环境光与 mode0/1/2/10 有序绘制命令。对象使用调用方提供的非零令牌；scene 负责真实灯资源与 GPU 灯光快照。投射阴影命令不代表效果已经实现。见 `reports/lighting-pass.md`。

`rain`是原51a190绘制入口的天气策略，独立于background的模拟dt；与其他系统共享CRT RNG，scene负责rain.tga与有序GPU提交。见`reports/rain.md`。

`draw_dispatch`恢复51c736根/灯模式选择及4d9733按声音播放状态接管的服务调用顺序。scene提供实际准备、事件渲染、音量和常规灯光绘制服务；缺失即失败，不把空服务当作游戏功能已完成。见`reports/draw-dispatch.md`。

`item`恢复4ec9f0/4ef168配置状态和4f5c6a拾取时序；声音/提示是显式服务，缺失不执行。scene负责模型全速显示与下一显示阶段隐藏，见`reports/items.md`。
