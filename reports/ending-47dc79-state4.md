# 47DC79 state4 控制器边界

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 重新按原版 `47DC79` 跳表区分 state4 与 state8。原有
  `ending_auxiliary_controller` 实际是 state8 的媒体/动作前缀，已在注释中
  修正归属；新增 `runtime/game/ending_auxiliary_state4.{h,c}` 承载 state4
  的五个子状态 `6C7F50=0..4`。
- state4 的相机轨道、预设相机、FOV `.2` 递减、目标捕获、媒体 owner0/1
  等待、`481E0A` 效果参数、`721EEC/722100/721E0C` 写入顺序已经按汇编
  接口建模。`previous_flow=0x18` 与 `8` 的入口分支分别保留；未知子状态
  保持原版空操作。
- `ending_normal_session` 已把 4D39E6 的相机轨道、A_kuch 跟随目标、面部/眼睛、
  音频状态和共享 ending 字段接到该控制器。state0 仍只做原版的一字节段间转移。
- state4 开头原版还会把角色对象的 `+31C`/`+3B8` 运行时字段转移到
  `+328`/`+3C4`。当前可移植 `BkActorPose` 没有这两个字段的所有者，因此
  `prepare_actor` 被定义为强制服务；生产分派暂传 `NULL`，会明确失败并停在
  该边界，不能把相机状态机单元通过当成可玩的结局流程。

## 验证

- 普通构建：`bk_game`、`test-ending-auxiliary-state4`、`ending-normal-probe`
  通过。
- 普通 CTest：`ending-auxiliary-controller`、`ending-auxiliary-state4`，2/2
  通过；覆盖相机完成、媒体忙/闲、子状态复位和缺少角色服务的显式拒绝。
- ASan/UBSan 构建及同两项 CTest 通过，运行使用
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`。
- 指定 Mesa NVK Switch 交叉构建通过，Mesa commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，未定义符号为 0；构建目录
  NRO SHA-256 为
  `f9203abcb5ce80c40b67c2462930583213a7e3b572c7f66bc15904266f221c75`，
  大小 16,044,088 字节。

## 尚未完成

角色运行时字段服务完成前，真实 47DC79 state4 入口仍按设计失败；state1/5/6/7
及 state8 后续、其它结局 loader、自然结束/解锁、Switch 实机验收也不在本次范围。
没有生成新的整包或推送 GitHub；Mac 防休眠进程保持运行。
