# 47DC79 state5/6/7 结局序列

2026-09-30。本阶段把固定中文版 EXE 的 `47DC79` 状态 5、6、7 接入 `scene/ending_normal_session`。模块位于 `runtime/game/ending_auxiliary_sequence.{h,c}`，只接收 game 层的时序和普通提交类型；模型时序在 scene 适配器中转换，未把 `model`、Vulkan 或 libnx 依赖带入 game 层。

## 原版边界

固定原版 SHA-256 为 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。`47DC79` 跳表中的状态 5、6、7 分别进入 `47E880`、`4800B8`、`480A8F`；状态 6 的子状态 1 直接进入状态 7 尾部。代码保留了原版的：

- 初始/中间/最终效果音表、五组相机轨道和普通恢复相机；
- state6 的目标与轨道保存、语音/表情切换、state7 三次 pass 及结束后的相机和 toggle 恢复；
- state6 子状态 1 的 `48079D` 语音结束尾部：播放中继续检查末段效果阈值，停止后按原顺序写记录、黑幕、清理 latch 并回到 state4；它不跳入 state7；
- state5 子状态 0..4 的媒体等待、片段请求、计时结束、记录写入和黑幕转场；
- group3 的四段效果音互锁和原始阈值比较，含等号通过语义；
- state5/6/7 的 voice、动画重启、effect 状态/暂停/重播服务顺序。

state5 group0 的 `0x722D54` 经全局槽位和步长核对为 `0x722574 + 7*0x120`，即结局效果表第 8 项 `se207.wav`。原版在该分支直接播放这个缓冲；场景适配器保留独立的特殊回调，但通过共享 48 槽结局音频 owner 的效果 7 真实重播，不再把它当成未绑定资源或另建重复 owner。

新增的 state6 相机保存数据放在 `BkEndingState` 追加区，未改变已有 `BkEndingRetained` 的已验证布局。state8 的既有 toggle 和表情恢复字段仍借用原 retained owner。

## 验证

`tests/test_ending_auxiliary_sequence.c` 覆盖 state6→7 相机保存、state6 子状态 1 结束尾部、state7 pass/恢复、state5 结束/记录、group3 效果阈值、特殊音频效果 7 重播和缺失服务边界。普通与 ASan 均通过。

固定 EXE 状态 oracle 对 175 个映射区域执行 6000 个随机前缀，比较 281,868,000 字节，最大误差 0；它验证的是既有 `4CC582..4CC7E6` 状态初始化/前缀，不把它误写成 state5/6/7 的完整原指令回放。

本轮结果：

- 主机 `test-host.sh`：普通 CTest 111/111，ASan CTest 102/102，Python 检查 29/29；
- `test-ending-auxiliary-presentation`、`test-ending-auxiliary-state3`、`test-ending-auxiliary-sequence`：通过；
- 指定 Mesa NVK Switch 构建：通过。`build-switch/biko3-preview.nro` SHA-256 为 `784d0019fbd5393b7e0637d9597b4f017439e4f023959c70724bb7b6fe0a5254`，ELF SHA-256 为 `bcbbc4ba945790a4619bec552e94c8bfed99595a7f835b706114932ded049e4a`；
- 本轮没有 Switch 实机回归，因此不能把交叉构建等同于实机声音、完整结局或全流程通过。

下一步继续接入 state5/6/7 的真实长流程，再继续自然结束、正式 gallery、持久解锁及整体验收。当前没有新增 Switch 实机回归；Mac 防休眠 PID 64008 继续保持。
