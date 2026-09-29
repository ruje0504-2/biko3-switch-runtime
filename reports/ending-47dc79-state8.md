# 47DC79 state8 前置控制器

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 将原先误按 state4 入口实现的 `ending_auxiliary_controller` 改为
  `47DC79` state8 前置段；state4 仍由
  `ending_auxiliary_state4` 负责。
- 按 `47E474` 的原顺序恢复 owner0/owner1 媒体等待、clip5 请求、
  表情、`54F8E0=0)、state/index/camera 缓存写入和 `481E0A(7,0,0)`
  语音调用。
- group0/1 恢复 `54E2BC` 音效索引 10/29、`7220F9` 保存后置1、
  特殊相机四元组、目标捕获、target_choice、camera_clip、相机表第一项
  和 mode4。
- group2/3 恢复 `54F7A0` 相机四元组、四个活动预设的第一选择、目标捕获、
  target_choice、camera_clip、相机表第一项和 mode2。
- group4 按原版在 cue7 后直接保留 state6，不改相机、目标选择或相机表。
- 场景层接入真实 ending 音频、面部/眼睛、主角色 slot5、actor forest 目标、
  `BkMenuCamera` 和 `BkEndingCameraPresets`；未实现的 state3 仍会显式失败，
  所以该段不会伪装成完整可玩流程。

## 验证

- 普通 CTest：109/109。
- 普通/ASan state8 控制器单元测试通过，覆盖五个 group、媒体忙等待、
  group4 原子边界、目标/相机/音效/保存 toggle 的调用顺序。
- 普通/ASan 实际 `local/game/MAINDIR/Data` ending 资源探针通过；
  普通 story scene probe 64 帧、10335 draws、528 skin instances、63360 audio frames
  通过，最终 RGBA 一致。
- 固定 EXE UI oracle 通过：720 帧、267840 vertices、max error 0。
- 指定 Mesa NVK 交叉构建通过，未定义符号为 0；本次 NRO SHA-256 为
  `ae71e9a6d808788f12ed9e1a1540138db962ecde4a8180f00d90730cfd36d944`。

## 尚未完成

state3 的 `481EA5` 完整判定与跳转、state5/6/7 后续、
其它结局 loader、自然结束/解锁写回、Switch 实机验收仍未完成。
本次只做本地提交，不推送 GitHub、不生成整包；Mac 防休眠保持运行。
