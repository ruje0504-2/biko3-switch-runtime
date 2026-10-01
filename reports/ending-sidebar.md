# 结局侧栏物品、姿势切换与摇杆动作

2026-10-02，最新修复：独白开关启用后，phase4 动作切换写入 `auxiliary.index=-1`；尚在播放的语音1仍触发副镜头绘制，产生 `ending special: invalid secondary camera index` 并退出。实机诊断日志和主机真实侧栏/拖动输入均出现相同错误。

生产渲染适配在索引恰为 `-1` 时使用当前主镜头，保持事件、视频、语音状态及音量处理顺序。没有改写流程索引、钳制到任意镜头或放宽其他非法索引检查；严格原指令调度入口不改。原指令会对该索引读取镜头表之前的全局字节，主镜头处理是明确的移植兼容修正。

原版 `hs_43.tga` / 第10号侧栏控件是「モノローグ」，汉化贴图显示「物品」，实际控制 `control.toggles[7]` 的独白语音。前两个附件开关为 `hs_35/37.tga`，分别控制人物材质，不能替代独白开关的复现。

修复后普通/ASan 各通过 661 帧实际输入：state1→state3→state8→state7，600 帧保留 `-1`，其中230帧与独白播放重叠，GPU释放回基线；`draw-dispatch`、`ending-special` 两项单元检查各构建通过。新探针模式为 `auxiliarymonologue`，入口和库存为显式夹具，随后不注入动作状态或镜头索引。未扩展完整剧情审核或实机运行验收。

人物左摇杆仅选择动作已完成插入后的手动阶段（gate=3、mode=1/2/4/6）加快2倍；开场、姿势菜单、其他人物动作及触屏/鼠标原速，L/R镜头原有6倍不变。此前普通/ASan的第二角色资源回归各通过六次拖动换姿势与47项实际动画速率/阶段检查，该证据与本次镜头修复回归分开记录。

正式NRO为1.0.79，BKTR更新为1.0.79cn，文件日志关闭。最新打包记录见 [更新包](nsp-update-1.0.79.md)。

```sh
build/ending-sidebar-probe local/game/MAINDIR/Data 1 auxiliarymonologue 0 local/patch-speed/patch.pp
ASAN_OPTIONS=detect_leaks=0 build/asan-vk/ending-sidebar-probe local/game/MAINDIR/Data 1 auxiliarymonologue 0 local/patch-speed/patch.pp
```

以下为首次交付的历史检查与 6 倍版本说明。

2026-10-02，首次版本1.0.79的侧栏附件与姿势切换检查。

实际资源和输入检查复现了三处会终止当前帧的问题：

- 选择结局及辅助场景没有创建侧栏特殊视图所需的第二份 GPU 视图，报 `special view storage unavailable`。现在沿用已有的双视图构造，分别保留几何快照。
- 共用侧栏的姿势切换回调为空，报 `missing actual auxiliary transition495d92`。现在接入已实现的 `selected_manual`，继续按原状态门限接受或拒绝切换。
- 隐藏背景后，渲染器仍要求背景可见或提前上传尚未发布的角色缓存，报背景拓扑或 `later flush changed prepared actor worlds`。现在保留空背景 flush，等首个实际绘制遍历发布缓存后再上传；原拓扑和后续快照检查继续保留。

左摇杆驱动人物拖动动作的增益设为 6。普通结局在模型手动拖动处应用，选择结局在动画拖动处应用，第三类场景在原运动限速之后放大动画推进。触屏、鼠标、菜单光标、自动播放时间和已有 L/R 镜头速度保持原行为，原动作幅度边界仍有效。按键说明不增加倍率文字。

最终主机构建 18 项、ASan 7 项全部通过，共 2,624 次侧栏点击、30 次实际拖动换姿势、375 项 30/60/120 Hz 增益检查。主机构建覆盖五角色选择结局两种入口、五角色普通结局及第二角色第三类/辅助场景，另检查第二角色日文资源；ASan 覆盖第二角色各相关入口及第五角色选择结局。相同七项的普通与 ASan 结果一致，GPU 分配回到基线，无 ASan 错误。三个相关单元检查通过。

入口、库存及手动状态为显式夹具；侧栏点击、连续滑动、资源换场、音频及 Vulkan 绘制走实际生产路径。检查使用 Apple M4 / MoltenVK，不是完整剧情审核或 Switch 实机验收。失败的旧运行及中途终止的检查不计入上述结果。

可复跑第二角色入口：

```sh
build/ending-sidebar-probe local/game/MAINDIR/Data 1 selected 1 local/patch-speed/patch.pp
```

终态计数及构建摘要见 [验证记录](ending-sidebar-verification.json)。交付 NRO 为 1.0.79，BKTR 更新包为 1.0.79cn，原日文本体仍为 1.0.78。
