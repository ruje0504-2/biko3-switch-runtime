# 发现结算与区域边界/提示音

`game/npc_detection`恢复完整4f3c80：玩家live action slots11..14或area>=8先清NPC detected(+318)，即使已有outcome；否则detected恰为1且outcome0才设behavior4/outcome2。噪声+319恰为1，或NPC action等于+68/+6c绑定，改outcome3。直接操作既有NpcSpatial/Interaction，不复制另一个检测状态。24,000次无hook原函数对照，含随机其余对象字节，12,566清检测、1,427次outcome变化（804个code3），完全一致。

`game/area_boundary`恢复4f64ae、4f5e24、4f607a与完整4f3d34规则。16道具按原槽序更新下次presentation使用的hidden；11个特殊人物/区域组合覆盖边界并保留ambient gate。普通train kind1检查车头/中点/车尾，偏移±400，offset先float、加位置后不提前float舍入；普通其他kind可清掉此前train写出的共享环境音门控。矩形包含边界，int32界值按double比较保留x87语义。NPC不使用特殊边界；不存在时保留hidden，该旧值仍能触发玩家同名墙response1。路点声音查询用NPC人物ID，文件选择用全局人物ID；NPC和玩家两个独立已播放字节按原exact-byte条件门控。输出顺序NPC后玩家；NPC空间衰减6、effect master，玩家中心pan。规划拒绝保持CPU状态，声音必须由后续服务实际执行。

`scene/area_audio`载入bk3_02/se304.wav、se155.wav、se156.wav。NPC区域声音使用原+568独立通道，区别于脚步+448和口型语音+984；玩家使用现有+458 effect通道，`player_audio_effect`同时维护voice-present标志。命令整体预检再按顺序应用，音频失败终止帧，不声称回滚已经提交的声音。模块借用mixer和player service，调用者保留明确且不重叠的NPC通道；destroy仅释放clip，stop只停止单独NPC通道，玩家由原服务管理。

验证：

- 17,280原道具边界及17,280原NPC矩形查询，包含边缘、倒置矩形、大于2^24的int32界值及原CRT sin/cos；hidden/gate完全一致。
- 10,472路点声音门控、77人物/区域文件查询，无hook完全一致。
- 完整4f3d34、4f64ae、4f5e24、4f607a、50d2a0执行4,500帧，35,911个present道具、532声音（41帧NPC/玩家同时），原状态/文件/空间音量/pan/调用顺序全部一致；仅声音加载/播放和DS setter是平台边界。
- 10真实路点提示、3实际单声道PCM16/22050Hz资源，NPC/玩家同时混音、只停NPC、再停玩家、非法第二命令不重启首命令。82,320采样（54,384非零）与原素材叠加/限幅逐样本相同，普通和ASan hash均b4906d50c1cbe525。这是离线sink，不宣称Switch真机可听验证。
- 单元验证共享状态、最后道具覆盖、缺NPC保留、触发一次及后续无效几何的原子拒绝；50主机CTest、28Python、42CPUASan全部通过。
- 指定Mesa NVK交叉构建通过，undefined symbols0；NRO `687b520d254877dad7352750708c34c184b38a2d179c7d2222f45cb7db7ba0ca`。这些组件仍待中央游戏帧实际装配，未接应用的新组件被移除后NRO与提示阶段相同，不能称完整可玩移植。SD归档不变。

下一项4f4306道具交互/事故/可用物体矩阵，随后51a682中央帧装配。其直接DirectSound Play/Stop不会自动rewind，需要补足保存播放位置的音频transport后再接，不能误用现有restart-at0。Mac防休眠继续，完整任务/场景/实机仍未完成。
