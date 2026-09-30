# 47DC79 state5/6/7 结局序列

2026-09-30。本阶段把固定中文版 EXE 的 `47DC79` 状态 5、6、7 接入 `scene/ending_normal_session`。模块位于 `runtime/game/ending_auxiliary_sequence.{h,c}`，只接收 game 层的时序和普通提交类型；模型时序在 scene 适配器中转换，未把 `model`、Vulkan 或 libnx 依赖带入 game 层。

## 原版边界

固定原版 SHA-256 为 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。`47DC79` 跳表中的状态 5、6、7 分别进入 `47E880`、`4800B8`、`480A8F`；状态 6 的子状态 1 直接进入状态 7 尾部。代码保留了原版的：

- 初始/中间/最终效果音表、五组相机轨道和普通恢复相机；
- state6 的目标与轨道保存、语音/表情切换、state7 三次 pass 及结束后的相机和 toggle 恢复；
- state5 子状态 0..4 的媒体等待、片段请求、计时结束、记录写入和黑幕转场；
- group3 的四段效果音互锁和原始阈值比较，含等号通过语义；
- state5/6/7 的 voice、动画重启、effect 状态/暂停/重播服务顺序。

state5 group0 的 `0x722D54` 是原版独立 DirectSound 缓冲，不与结局效果音表别名。当前场景适配器对此服务明确返回失败，避免以错误音效伪装完成；该特殊缓冲的资源映射仍是后续工作。

新增的 state6 相机保存数据放在 `BkEndingState` 追加区，未改变已有 `BkEndingRetained` 的已验证布局。state8 的既有 toggle 和表情恢复字段仍借用原 retained owner。

## 验证

`tests/test_ending_auxiliary_sequence.c` 覆盖 state6→7 相机保存、state7 pass/恢复、state5 结束/记录、group3 效果阈值和特殊音频失败边界。普通与 ASan 均通过。

固定 EXE 状态 oracle 对 175 个映射区域执行 6000 个随机前缀，比较 281,868,000 字节，最大误差 0；它验证的是既有 `4CC582..4CC7E6` 状态初始化/前缀，不把它误写成 state5/6/7 的完整原指令回放。

本轮结果：

- 主机 `test-host.sh`：普通 CTest 111/111，ASan CTest 102/102，Python 检查 29/29；
- `test-ending-auxiliary-presentation`、`test-ending-auxiliary-state3`、`test-ending-auxiliary-sequence`：通过；
- 指定 Mesa NVK Switch 构建：通过。`build-switch/biko3-preview.nro` SHA-256 为 `2a4c5931eacbe112d5186b748d5388c530ae88433fa62b1d0bb3027959db4aa8`，ELF SHA-256 为 `2132fee8b6348719c7ed4869221e0b9fce686efe2f05240e32e47bfd695679f9`；
- 本轮没有 Switch 实机回归，因此不能把交叉构建等同于实机声音、完整结局或全流程通过。

下一步接着恢复 `722D54` 特殊音频资源和 state5/6/7 的真实长流程，再继续自然结束、正式 gallery、持久解锁及整体验收。Mac 防休眠 PID 64008 继续保持。
