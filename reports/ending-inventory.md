# 结局背包共享、辅助 state1/2 与禁用目标悬停

2026-10-01。原版 `71BCDC..71BCE0` 是游戏拾取与存档共同使用的五个背包字节。此前结局另建了零初始化的 `ui_item` 和两个选项副本，因此持有第一类道具时仍显示无道具图标，并走错后续分支。

## 实现

`PlaySession` 将 `game_state.pickup.collected` 借给 `BkEndingNormalFlow`；结局 UI、第三类结束控制和各加载器读取同一个背包。独立诊断入口使用自己的五字节存储。scene 不依赖 save，存档格式没有改变。

原 `4CC878/4CC87F` 在鉴赏入口将第二、三个字节设为 1；`4CEB27` 只在 previous flow 为 `0x18` 时清理这两个字节。清理放在逻辑停止时，只执行一次；旧 GPU 快照延迟销毁不能再次清理新场景的背包。构造失败则恢复入口改动的两个字节，不声称回滚所有进程状态。

真实输入测试还复现第三类控制器的原生未定义路径：光标随镜头缓动进入已禁用目标后，`4771D6/477243` 会到达 `477305`，读取尚未初始化的语音编号。严格 CPU 入口继续拒绝该路径；生产入口 `bk_ending_tertiary_control_play` 仅跳过这两个禁用目标的无效悬停语音及相关表情/锁存写入，保留目标、UI、确认和选择处理。点击禁用目标仍不执行动作或写记录。此项是明确的兼容修正，不是对原版未定义行为的等价证明。

## 47DC79 state1/2 修复

持有道具路线进入 phase4 后，固定 EXE 的 `47E1DA` 检查的是全局 `72210C` 菜单打开状态，移植先前误读成了 `722100` 的 `pending`。现已把 `BkEndingState.open` 接到 state1；同时恢复了媒体忙/镜头模式只跳过拾取、仍继续 idle 查询的控制流，所有按键服务按原版只取 AL，state2 保留两次低 AL 查询。

菜单服务按原边界拆成 `47CB41` 命中区域和 `481C2C` 菜单布置：先完成有效区域判断，再写 state3，清 choices 并执行三点布置，随后才发 voice3、request2、expression5/4/0 和镜头事件。state5/6/7 的效果槽 presence 也改用 2..46 的完整 ending mixer 服务，不再误送给只允许语音槽 0/1 的 state4 服务。

## phase4 后续与 state5 交接

正式应用探针现在会在 item1 路线进入 phase4 后继续使用真实目标拾取、菜单点位和确认输入，直到生产 flow handoff；不会直接写 phase、state 或进度。state5 对 group2/3 的 clip6 循环入口按原版单独处理，并恢复 group2 的两个循环效果音；state5 尾部也保留了 group1..4 的 clip8→clip9 交接。

## 验证与范围

- 12 项实际应用普通/ASan 配对：五角色各有/无道具，再加第一角色道具字节为 2、255。每构建 64,002 帧、61,650 输入步，其中 item1 phase4 后实际辅助输入 15,923 帧；60 次真实图标切换、24 次生命周期检查和 12 次失败构造回滚通过，状态及 PCM 摘要逐项一致。只有字节恰为 1 时进入 phase4，其余进入 phase6；入口的故事完成状态与背包仍是显式夹具，随后通过生产输入执行，没有强制阶段或进度。
- 每构建 12 次实际资源缺失构造失败、24 次实际鉴赏/故事入口、重复停止及旧 owner 延迟销毁；只修改原版规定的背包字节，结束后 GPU 分配回基线。
- 原指令普通/ASan 配对：256 次初始化保留、1,024 次入口五字节对照、65,536 次释放前缀。资源和淡出仍是显式服务边界。
- 严格第三类控制器普通/ASan 各 12,000 帧、22,155 调用、514 失败前缀、1,391 活值变更、20 拒绝，保持此前摘要。
- 悬停兼容专项普通/ASan 各 1,200 个有定义原版帧、720 个禁用目标案例、540 个确认、38 失败前缀；另一个无关未定义目标仍拒绝。禁用案例的 x86 对照明确跳过 `477305..4773A9`，单独记录为修改过的参考控制流。
- 相关 CTest 普通/ASan 均通过，29 项 Python 检查保持通过。完整库存矩阵日志见 `build/validation/ending-runtime-0f5two7o/`，机器可读结果见 `reports/ending-inventory-verification.json`。

应用配对日志在 `build/validation/ending-runtime-0f5two7o/`。12 项背包检查普通/ASan 共 24 次全部通过；持有 item1 的五角色均从 phase4 完成到 phase6 handoff，其余字节进入 phase6，状态与 PCM 摘要逐项一致。测试跟踪实际动画目标并保留必要镜头输入，不修改生产逻辑来满足断言。

## 自然 item1 路线

2026-10-01，维护工具 `tools/natural_item_save_probe.c` 已完成第一角色 group0/item1 的实际拾取，以及 area2、area3、area4 的连续交接、保存和独立读档。入口使用生产 area loader 进入 area1，并固定一次初始随机种子；之后不写 phase、progress、action、outcome 或背包。NPC 视线、碰撞、拾取、出口和菜单均走生产代码。

`python3 tests/check_natural_item_route.py local/game/MAINDIR/Data` 已接入 `test-host.sh`，普通/ASan 各完成六个独立进程：

- `produce`：16,486 帧，拾取 item1，保存 area2 后关闭菜单并继续到 area2；此步不称为从磁盘读档。
- `reload`：2,441 帧，通过暂停/读取菜单从磁盘恢复 area2。
- `continue`：7,368 帧，从磁盘恢复 area2，等待 NPC 离开，绕过路障，经 `Mesh_End_Hantei4` 完成下一场景交接；覆盖保存 area3，关闭菜单后继续运行。
- `reload`：2,453 帧，新进程从磁盘恢复 area3。
- `continue`：7,088 帧，从磁盘恢复 area3，等待 NPC 离开，经西侧 `Mesh_End_Hantei3` 交接并保存 area4。
- `reload`：2,447 帧，新进程从磁盘恢复 area4。三次只读加载均不修改存档文件。

每构建 38,283 帧，五个背包字节始终为 `01000`；十二个完成的进程退出时 GPU 分配回到 renderer 基线，无 ASan/UBSan 诊断。原运行在最后一个 ASan 读档进程中断，前十一项已由顺序驱动器接受；仅补跑未完成的最后一项，保留原部分日志。恢复记录为 `build/validation/natural-item-nc7pi1ah/verification-resumed.json`，摘要见 `reports/ending-natural-item-route-verification.json`；没有将未生成的原总报告称为通过。较早探针误把进入失败流程当作 continue 成功，已改成只接受下一场景成功交接。

## 尚未完成

本批已完成显式库存入口下的 phase4 实际交互、state5/6/7 交接和五角色普通/ASan 矩阵；另已完成 group0/item1 的自然拾取→area2→area3→area4 连续保存/读档路线。

单角色无道具的完整第三类流程在本地探索中已普通/ASan 配对完成：13,955 录制/保存/结束对话帧、13,473 新进程回放帧、两次选择资源加载；记录与解锁文件逐字节相同。该工具尚未纳入维护入口，不能扩展为五角色全分支验收。详细接续：`local/ending-third-natural-next/next.md`。

自然路线的完整故事交接、其他角色/道具的自然路线、完整原版三维像素对照与本批 Switch 实机仍未验收。用户确认的原版 PC 光影闪烁修复保持关闭，不能重新列为未解决。

phase4 验证时的指定 Mesa NVK 产物为 16,240,696 字节，SHA-256 `24a5de85d24861e629f57340627f1d02f1cb0327c2d7b5e8f6183ec0768e4271`，ELF 未解析符号零。这是后续 NEON 优化前的历史产物；本次仅维护主机探针，没有重新生成 NRO。

本轮授权的一次 GitHub 增量已推至 `6fbe62e`；之后只做本地提交，不追加推送、不整包。完整目标未完成，中断后恢复的防休眠 PID 9101 保持。
