# 结局特殊双镜头调度与局部深度清除

本阶段完成 `game/ending_special` 的原 `4d9898` 调度、原生相机表和显隐名字，以及 `render` 的 Vulkan 视口深度清除。尚未把特殊场景的实际节点、材质、相机和两套 GPU 快照组合成生产适配器；不能据此宣称完整 flow16 已接入。用户确认的 Switch 实机正常范围是先前交付包的雨天。

## 已恢复行为

- 第一遍按原描述符绘制，然后保存当前相机位置、前向和上向，执行 EndScene、切换副视口、清深度、BeginScene。
- 三次基础名字查找保留空名字、重复命中和未命中语义。临时显隐按原顺序提交。基础节点恢复为共享字节 `7220f9`，并非其各自旧值；组0两个额外节点没有恢复分支。组1/2/4按第二遍绘制后的实时状态决定恢复。
- 组4的两个原材质名字地址 `576010/576024` 实际存放相同字符串。隐藏时设置 diffuse alpha0，恢复时设置1，不能误读为两个不同材质或任意旧 alpha 的回滚。
- 位置来自加载时复制的五组108行表。目标来自旧发布节点缓存；按角色、阶段和索引，偏移 Y、X、Z 或不偏移。第二次相机复位前捕获位置、之后读取目标和偏移，保留服务之间实时状态变化。
- 第二遍保留描述符前20槽和 shadow 字段，清空20..51槽，设置 mode2、主体，以及 phase1/8存在的辅助根。修改保留在调用者描述符中。
- 恢复保存的主视口，重新设置相机位置/方向并发布；不是直接复制完整相机矩阵。服务失败保留已执行前缀并终止；缺失所需服务会明确失败。

所有原生地址限定于 SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e` 的中文 EXE。

## 深度清除证据和实现

恢复链为 `42a08b → context vtable540270+54 → 52ab4c → device vtable+28`，后者是 D3D7 Clear。原构造 `529b9c` 设置深度默认值1，调度传入 flags2、count0、rects NULL。`4a9f10` 只复制六个视口字段，初始化副视口是 `(0,0,640,480,0,1)`，没有隐含缩放。生产适配器仍须按当前输出尺寸显式处理视口映射。

`flags2` 为深度清除的定义见 [Microsoft D3D types](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/d3dtypes.h#L825-L828)；Clear 的接口顺序见 [Microsoft D3D7 interface](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/d3d.h)。无矩形清除受当前视口约束的行为另有 [Wine D3D7 原生兼容性测试](https://github.com/wine-mirror/wine/blob/master/dlls/ddraw/tests/ddraw7.c#L15172-L15227) 支持。没有复制外部实现代码。

`bk_renderer_clear_depth` 在当前 Vulkan render pass 内记录 `vkCmdClearAttachments`，只清当前视口的深度。保留颜色、视口外深度和绘制状态，不分配、不等待、不读回、不提交或 present。无效深度和非活动帧在记录命令前拒绝。它提供真正的 GPU 操作，但未将 D3D Begin/End 边界伪装成完整特殊场景实现。

## 验证

|检查|结果|
|---|---|
|完整原4d9898调度回放|12,000帧、319,701服务、24,000绘制、62,178显隐、605材质调用，逐字节一致|
|原数据|540镜头行、150名字一致|
|实时状态/缺失/别名|绘制和复位回调中改变共享状态；覆盖空名字、缺失命中、重复节点、非布尔恢复字节|
|原设备包装|54 Begin/End门控、2,000 Clear参数转发、500视口构造一致|
|失败检查|35处逐项注入失败、非法索引、缺失材质服务，普通和ASan通过|
|Vulkan深度清除|120帧、921,600输出通道、120次capture/LOAD恢复，颜色/深度遮挡/视口恢复精确，分配稳定|
|普通/ASan GPU|日志一致；ASan `detect_leaks=0`|
|工程检查|2项CTest、2项架构规则、指定Mesa NVK交叉构建通过，无未解析符号|

原调度回放中 GPU、节点和相机服务是观测边界，证明顺序与参数，不证明原版矩阵运算、Windows光栅化或完整结局画面。GPU检查使用合成颜色矩形，不输出游戏画面。

源码 NRO 为 `947834127921ae617f366bd2f48e623c9c1a6f88b490f053da266da11f8d7c40`。两个已交付ZIP保持原摘要，本次没有重新打包或追加实机结局验收。

下一项是实际场景适配：保持相机 local/world/parent-cache 和预遍历顺序，对两个视角分别保留几何、材质、灯光、队列快照，接入已完成调度及深度清除。常规 renderer 的 mode1/可见末尾背景约束仍然有效，不能绕过或让第二次 prepare 使第一份批次失效。随后继续剩余结局子阶段、UI、flow16生命周期以及设置/鉴赏收尾。
