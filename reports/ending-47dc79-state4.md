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
- 已用固定 EXE 和实际 `h01_00.xan` 资源确认这些偏移的语义：
  `721B28+31C/+328` 是 slot2 的 authored start/source，
  `721B28+3B8/+3C4` 是 slot3 的 authored start/source，并非角色包围盒。
  `prepare_actor` 现在通过 `BkActorPose → BkModelPlayback → BkClipPlayer`
  只恢复 slot2/3 的 `source`，保留 elapsed、rate、loops、请求和过渡状态；
  生产分派已接入主角色姿态，失败时仍会明确返回错误。

## 验证

- 普通构建：`bk_game`、`test-clip`、`test-ending-auxiliary-state4`、
  `ending-normal-probe` 通过；普通 CTest 的 `clip-scheduler`、
  `ending-auxiliary-controller`、`ending-auxiliary-state4` 为 3/3。
  `test-clip` 覆盖 source 批量恢复、其他时间线状态保持和非法 slot 的原子拒绝。
- ASan/UBSan 的 `test-clip`、`test-ending-auxiliary-state4`、
  `ending-auxiliary-controller`、`ending-auxiliary-state4` 和 `clip-scheduler`
  通过；普通/ASan `ending-normal-probe local/game/MAINDIR/Data` 均通过，运行
  使用 `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`。
- 指定 Mesa NVK Switch 交叉构建通过，Mesa commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，未定义符号为 0；构建目录
  NRO SHA-256 为
  `e048269bc45a250e51c9b3420248107cd30df74e760417bc7525024264719ef3`。

## 尚未完成

真实 47DC79 state4 的前置 source 恢复已接入；state1/5/6/7 及 state8 后续、
其它结局 loader、自然结束/解锁、Switch 实机验收仍未完成。
没有生成新的整包或推送 GitHub；Mac 防休眠进程保持运行。
