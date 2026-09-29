# 结局选择阶段：输入与动画源时间

2026-09-29。本项恢复原 `495469` 和 `4952C8`，并接入 `scene/ending_selected_assets` 的真实主体动画描述符。完整 `48E75B` 动作控制器、选择阶段的生产场景及后续自然结束链仍未完成；不能把本项视作新的可玩分支。

原地址只适用于固定中文 EXE，SHA-256 为 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。资源组合使用 `local/game/MAINDIR/Data` 的日文资源；不宣称与未验证的日文 EXE 等价。

## 原版规则与实现

`game/ending_selected_motion` 只计算源时间，不请求片段、不推进动画、不提交世界矩阵，也不拥有资源。

`495469` 使用旧投影点 `6AFD38`、第一个菜单中心 `7220C8` 和已捕获输入的第9/10字。坐标差先按有符号32位回绕再转换为 float，平方和先存 float 再开方。指针位于锚点半径以内时，源时间为两段 float 中间值计算后的结果加固定 **30**，不能改为加片段 start；位于边界或外侧时恢复 start。锚点重合时，原未使用的除零结果不影响最终 start，不人为引入失败。

`4952C8` 的运动量按 **float** 传参，原 `47A5B0` 调用双精度绝对值函数；不能照搬其他阶段的整数运动量接口。duration 为10/15/20时系数为 `.00005f`，30/40为 `.00008f`，60/80为 `.0002f`，其他值为 `.0001f`；随后按原顺序乘 `.5`、片段跨度及 `rate * 60`。本函数没有额外秒数因子。

`B53C38 == 0` 时只消费 X，`6EA314 != 0` 表示递减并在 start 钳制；另一方向递增，不在 end 钳制。`B53C38 != 0` 时使用 `abs(X) + abs(Y)` 递增，忽略方向字。普通路径不读取 Y，因此其未使用的非有限 Y 不导致可见失败。输出溢出或实际使用的非有限输入按可移植接口契约拒绝，并保留原描述符。

场景接口 `bk_ending_selected_assets_pointer` / `bk_ending_selected_assets_drag` 捕获真实主体的 active slot、timing 和 prediction，完成计算后仅提交 `BK_CLIP_EDIT_SOURCE`。elapsed、请求/过渡状态、局部姿态、world/parent-world 缓存、ANIM/MATA/MORP 采样缓存及背景所有权保持。后续采样必须由已有普通或受控推进接口显式执行。

## 已结算验证

纯数值配对记录：`build/validation/ending-selected-assets-necgz8ab/result.json`。普通与 ASan/UBSan 各16,534次径向控制、17,066次拖动控制，共33,600次，包含1,600步连续操作；整个描述符除 source 外的字段均保持。942次坐标回绕、2,286次重合锚点、7,428次 start 不等于30的径向结果、1,334次未使用 Y、9,286次越过 end 的结果均按原指令比较，误差0。32项拒绝检查为独立可移植契约，不冒充原无效输入行为。

两种构建状态摘要一致：`9076df09fb6c7cfff61b89ba343afbf562783591af940166445433c5547134e3`。原数学及其子函数不挂替代 hook。该报告之后只增加了真实资源适配，三个纯数值源码/测试文件的摘要已再次核对一致。

实际资源首个入口的普通构建检查已通过：`local/ending-selected-motion-smoke-5xm28dnd/motion.json`，24步输入/采样组合、81,270份矩阵比较，误差0。

完整资产组合配对已结算通过；接续时已回读 `build/validation/ending-selected-assets-wqi3hmui/result.json`，其终态为 `passed: true`。记录目录中的原始命令、源码和资源摘要保持不变，不再将这一批检查列为运行中。该结果仅覆盖本项输入与真实资产组合，不表示 `48E75B` 控制器或选择阶段生产入口已经接通。

两项架构检查、指定 NVK 交叉构建及新 game/scene 静态库符号检查通过，见 `local/ending-selected-motion-build-kpadroxj/result.json`。ELF为AArch64，未解析符号0，`nvk_CreateDevice` 已链接。新 motion 接口尚未被应用引用，因此 NRO保持 `d48d42dbfc92fb52f9da3d7cbe0ad72aa79b31ccbc1d3c818a98d374cc78e44f`，15,953,976字节。没有生成新交付包，也没有推送。

复现入口：

```sh
local/venv/bin/python tests/check_ending_selected_assets.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' \
  local/game/MAINDIR/Data --suites motion,motion-assets --jobs 4
```

## 未完成范围与接续

下一项仍为完整 `48E75B` 的分支、独立保留状态、录制/音频/镜头顺序及原版对照，再绑定已存在的父控制、表现、辅助循环和真实资源接口。`4D1025` 的实际 UI/媒体/场景入口、`4D39E6`、正式 gallery、自然结束/持久解锁/返回及完整 Switch 验收继续保留为未完成。当前检查未覆盖实际控制器输入顺序或实机表现，ASan/UBSan关闭泄漏检测，不作 LSan 验收声明。

本项此前两批补充读取被自动安全检查以“无法确定请求的安全状态”拦截，未执行；上述成功执行的独立读取、修改和验证不受此记录替代。完整目标未完成，已有 Mac 防休眠进程60608身份已复核并保持。后续用户要求的 L/R 手动镜头已从3倍提升至原始速度的6倍，见 `reports/ending-camera-six-icons.md`；自动开场仍为原速。本项构建记录中的 NRO 摘要是镜头6倍修复之前的历史产物。
