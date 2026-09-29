# 47DC79 state3 父控制器边界

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 新增 `runtime/game/ending_auxiliary_state3.{h,c}`，恢复
  `47DC79` state3 的父层顺序：先调用 `481EA5`，再查询
  `4B76C2(0,3)`；按下后读取输入第 9、10 个字，使用
  `7220C8/7220CC` 的首个点和 `7389E8/53F410` 的严格圆形命中。
- `481EA5` 的圆形命中分支已恢复：request4、group0/2 与其他 group 的
  表情、group2 的 index=0x49、owner0 媒体等待、cue5、RNG `%1000<=5`
  和 `6C7F44` latch 驱动的 cue6。miss 分支已接入 group2/3/4 的首段效果阈值、
  一次性 latch 和 group3 的存在/播放状态门。效果比较按原版 `fcomp` 语义在达到
  阈值（含等号）时触发。active clip4 的 group2/group3 时间比、mode0..8 偏移表和
  目标/指针距离 source 插值已接入；插值前先将活动源写到 authored end，成功后恢复
  `0x320`（slot2 end）比较、cue3/cue4 pending 转移，距离门失败时按原版执行
  request2、表情、活动源回退、十字节 latch 清零和 54e2bc 表的两个效果停止。
- 命中分支按原版写入 state8、`721EDC=-1` 和 camera mode 0；未命中分支按
  原版顺序执行 group2 的 index 0x48、表情 `(7,3,1)`、clip1、cue1，随后
  写入 pending=1、state1 和 cached camera=-1。
- `ending_normal_session` 已注册 state3 父层、实时 slot51 几何、角色 clip、表情、语音、
  RNG、效果、活动源 end/source 回写、媒体状态和效果停止服务。缺少这些必需服务时仍
  在实际调用处明确报错，不用空回调替代原版副作用。

## 验证

- 普通 CTest `101/101` 通过；普通/ASan `test-ending-auxiliary-state3` 均通过，覆盖
  child→key→hit 顺序、命中 state8、未命中 state1、group2 index、输入坐标、481EA5
  命中前缀、三组效果阈值含等号、group3 播放状态门、active clip4 的 end/source 写入、
  成功 pending 转移、失败复位顺序和缺失 child 失败边界。
- ASan/UBSan 的 state3 单测通过；普通/ASan `ending-normal-probe local/game/MAINDIR/Data`
  均通过，普通 `ending-normal-scene-probe ... 0 --story` 通过 64 帧、10335 次绘制、
  528 个蒙皮实例和 63360 个音频帧。
- 指定 Mesa NVK Switch 交叉构建通过，Mesa commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，AArch64 ELF 未解析符号为 0；构建目录
  NRO SHA-256 为 `e2fa65f5678d37710fd2c2ef63fe1a4c060ffa8d4f78b576424357e7d1b0fd9d`。
- `git diff --check`、`sh -n test-host.sh` 和依赖锁 JSON 解析通过。

## 尚未完成

state3 的 `481EA5` 子控制器已覆盖当前固定 EXE 的完整分支前缀，但尚未成为可玩的完整
结局入口。state5/6/7、自然结束/解锁、其他
结局 loader、Switch 实机验收仍未完成。本轮只本地提交，不推送、不整包；Mac 防休眠保持。
