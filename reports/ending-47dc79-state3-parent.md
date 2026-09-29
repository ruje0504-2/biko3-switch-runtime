# 47DC79 state3 父控制器边界

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 新增 `runtime/game/ending_auxiliary_state3.{h,c}`，恢复
  `47DC79` state3 的父层顺序：先调用 `481EA5`，再查询
  `4B76C2(0,3)`；按下后读取输入第 9、10 个字，使用
  `7220C8/7220CC` 的首个点和 `7389E8/53F410` 的严格圆形命中。
- 先接入 `481EA5` 的圆形命中分支：request4、group0/2 与其他 group 的
  表情、group2 的 index=0x49、owner0 媒体等待、cue5、RNG `%1000<=5`
  和 `6C7F44` latch 驱动的 cue6。miss 分支已接入 group2/3/4 的首段效果阈值、
  一次性 latch 和 group3 的存在/播放状态门。active clip4 的 group2/group3 时间比、
  mode0..8 偏移表和目标/指针距离 source 插值前缀也已接入；后续媒体/状态转移仍单独拒绝。
- 命中分支按原版写入 state8、`721EDC=-1` 和 camera mode 0；未命中分支按
  原版顺序执行 group2 的 index 0x48、表情 `(7,3,1)`、clip1、cue1，随后
  写入 pending=1、state1 和 cached camera=-1。
- `ending_normal_session` 已注册 state3 父层、实时 slot51 几何、角色 clip、表情、语音、
  RNG、效果和 active clip4 source 编辑服务。481EA5 插值之后尚未恢复的媒体/状态转移会在
  实际调用处明确报错，保留原流程边界。

## 验证

- 普通/ASan CTest 均 `101/101` 通过；新增 `test-ending-auxiliary-state3` 覆盖
  child→key→hit 顺序、命中 state8、未命中 state1、group2 index、输入坐标、481EA5
  命中前缀、group2/3 阈值/latch/播放状态门、active clip4 插值顺序和缺失 child 失败边界。
- ASan/UBSan 的 state3 单测通过；普通/ASan `ending-normal-probe local/game/MAINDIR/Data`
  均通过，普通 `ending-normal-scene-probe ... 0 --story` 通过 64 帧、10335 次绘制、
  528 个蒙皮实例和 63360 个音频帧。
- 指定 Mesa NVK Switch 交叉构建通过，Mesa commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，AArch64 ELF 未解析符号为 0；构建目录
  NRO SHA-256 为 `eb5cdaa33e451249fe29373eca0aae2303eb4ee73d98cca0cf9efbffd4f6b063`。
- `git diff --check`、`sh -n test-host.sh` 和依赖锁 JSON 解析通过。

## 尚未完成

`481EA5` 仍需恢复插值后的媒体/状态转移、失败转移、group2/3/4 的剩余效果链和附属状态；因此
state3 尚未成为可玩的完整结局入口。state5/6/7、自然结束/解锁、其他
结局 loader、Switch 实机验收仍未完成。本轮只本地提交，不推送、不整包；Mac 防休眠保持。
