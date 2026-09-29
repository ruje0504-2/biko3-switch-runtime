# 47DC79 state3 父控制器边界

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 新增 `runtime/game/ending_auxiliary_state3.{h,c}`，恢复
  `47DC79` state3 的父层顺序：先调用 `481EA5`，再查询
  `4B76C2(0,3)`；按下后读取输入第 9、10 个字，使用
  `7220C8/7220CC` 的首个点和 `7389E8/53F410` 的严格圆形命中。
- 命中分支按原版写入 state8、`721EDC=-1` 和 camera mode 0；未命中分支按
  原版顺序执行 group2 的 index 0x48、表情 `(7,3,1)`、clip1、cue1，随后
  写入 pending=1、state1 和 cached camera=-1。
- `ending_normal_session` 已注册 state3 父层、实时 slot51 几何和已有表情/clip/语音
  服务。`481EA5` 尚未恢复，场景回调会在实际调用处明确报错，保留原流程边界。

## 验证

- 普通 CTest `101/101` 通过；新增 `test-ending-auxiliary-state3` 覆盖 child→key→hit
  顺序、命中 state8、未命中 state1、group2 index、输入坐标和缺失 child 失败边界。
- ASan/UBSan 的 state3 单测通过；普通/ASan `ending-normal-probe local/game/MAINDIR/Data`
  均通过，普通 `ending-normal-scene-probe ... 0 --story` 通过 64 帧、10335 次绘制、
  528 个蒙皮实例和 63360 个音频帧。
- 指定 Mesa NVK Switch 交叉构建通过，Mesa commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，AArch64 ELF 未解析符号为 0；构建目录
  NRO SHA-256 为 `39027aee9e0ca3bea6447e66a4427a2772e24a77676be0a377851bd99e8a3edf`。
- `git diff --check`、`sh -n test-host.sh` 和依赖锁 JSON 解析通过。

## 尚未完成

`481EA5` 子流程仍需恢复其角色 clip、命中后的媒体/表情、随机效果和 group-specific
分支；因此 state3 尚未成为可玩的完整结局入口。state5/6/7、自然结束/解锁、其他
结局 loader、Switch 实机验收仍未完成。本轮只本地提交，不推送、不整包；Mac 防休眠保持。
