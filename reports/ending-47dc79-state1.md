# 47DC79 state1/state2 控制器边界

本次固定原版为 `尾行3中文版.exe`，SHA-256 为
`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

## 恢复内容

- 新增 `runtime/game/ending_auxiliary_state1.{h,c}`，把 `47DC79` 的 state1
  从 state4 完成后的显式失败改为真实状态分派，并实现 state2 的两个
  `4B76C2(..., mode=0)` 等待查询。状态1保留原版顺序：`6C7F6C` 计时、
  `4AD2BF` 语音重启、`534A34 % 21 + 30` 随机延迟、slot1媒体等待、
  `47C334` 指针命中以及未命中时的 state2 键查询。
- 选中目标后，确认键查询按原版的三个别名短路；确认成立才进入
  `481C2C`。菜单区域复用已由固定 EXE 对照过的 `47CB41`，九个区域的
  `495125` 初始角度为 `{180,225,45,135,315,270,90,180,0}`，并保留
  三个点、choices 和 `54E2F8` 的写入边界。区域为 -1 时不伪造菜单，继续
  原版后续等待分支。
- 接入 `481E0A(3,0,0)` 的实际语音命名 `PH%d33%02d.wav`，写入现有
  `speech_names[0]` 后加载/重启真实 PCM；接入主体 clip2 的 configured
  request 与表情 `(5,4,0)`，最后写入 pending=2、camera mode=4、event=1。
  state4 保留的 `6C7F6C/54F8E0` 已加入 retained owner，482F91 和进程初值
  均恢复为 timer=0、delay=30。

## 验证

- 普通 CTest 109/109 通过，包含新增 `ending-auxiliary-state1`，并通过
  `test-ending-auxiliary-state1` 的选择、计时随机、服务顺序和 state2 回归。
- ASan `test-ending-auxiliary-state1`、`ending-auxiliary-controller`、
  `ending-auxiliary-state4` 定向回归通过；`git diff --check` 与
  `sh -n test-host.sh` 通过。
- 固定 EXE 的原版 UI oracle 以项目虚拟环境运行通过：zone 7200 次、菜单
  1440 次、pick 4800 次，`max_error=0`；这覆盖本次复用的 47C334/47CB41/
  495125 几何服务。
- 指定 Mesa NVK Switch 构建通过，commit 为
  `5dba7886c56460ff47c3038e323d07b9547d6212`，AArch64 ELF 未解析符号为 0；
  构建目录 NRO SHA-256 为
  `795ab8d074afb1d87e1f7d841db0ff8c3e592dfa2bf6e9db3e73b68d7233df8c`。
- `local/game/MAINDIR/Data` 的普通/ASan `ending-normal-probe` 均通过，
  `ending-normal-scene-probe --story` 通过 64 帧、10335 次绘制、528 个蒙皮
  实例和 63360 个音频帧；这验证了新增控制器不会破坏现有实际资源入口。

## 尚未完成

47DC79 state3/5/6/7/8 后续、其他结局 loader、自然结束与解锁、完整流程和
Switch 实机验收仍未完成。没有推送 GitHub，也没有生成完整交付包。Mac 防休眠保持。
