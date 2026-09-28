# 操作界面状态、绘制与会话绑定

scene/player_hud恢复4c9bf0、4cbca4、4cb902和4cc32f。49槽中槽2未构造；普通模式48资源，special字节恰为1时重新加载30资源，另外18槽保留之前的实际资源及状态。计时器deadline/armed与闪烁字节即使重载也保留。原显示状态为enter0/exit0/idle0，关出当帧复位缩放；数字重复绘制同一对象时保留每次位置和状态快照。counter>=100调用43、42、42，呈现100；负数按原有符号整数除法选槽，越过49槽范围明确拒绝。

709c68/64/5c/58状态条的驱动字节71b828对应player.scene.npc_in_view（目标是否在玩家视野内），不是角色姿态。保留平滑、长帧越界、储量耗尽outcome4、菜单/response门控、UV裁剪/倒向滚动、着色、三个500ms闪烁计时器、动作抑制与物品栏原顺序。prop提示使用42d56c投影；cover提示借用wall_available，NPC提示借用interaction.prompt，interface读取玩家interaction_mode。

player_hud_session把现有game_frame动作/视野/交互/物品状态和当前view/lens直接绑定，在开场完成后的phase1执行，再显示物品提示。视口使用游戏画幅局部坐标，由Vulkan视口加letterbox偏移。原始未命中过道具时，零矩阵投影产生NaN和未定义整数；在available!=1且投影失败时，本实现保留未使用的旧depth并显式返回unprojectable标记，已可用道具或活动相机无效则报错。此为明确的未定义数值处理策略，不声称与任意原未定义整数等价。

player_hud_render独立拥有真实bk3_00图像及每条draw的网格，避免数字同一槽被后一次更新覆盖。43ed45原float中间值、中心锚点、缩放、8位顶点色和alpha均保留。原D3D纹理寻址默认WRAP，52处SetTextureStageState调用未设置ADDRESSU/V，使用repeat线性采样（[Microsoft枚举文档](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms886612%28v%3Dmsdn.10%29)）。320帧全素材回读独立计算包裹采样与每次UNORM混合，最大误差1/255。

49d0eb截图服务的位置被保留为明确绘制边界：四个交互覆盖层之后，其余操作界面之前；普通模式要求实际服务回调，特殊模式不调用。此阶段只验证调用位置，未把截图服务标记为完成。下一项继续恢复暂停sy_99.bmp与拍照文件输出/截图的Vulkan时序，再接菜单、flow50和应用场景。

验证：60次真实原加载器（所有group、special0/1/2/255、640/1001/1280宽度）及9000帧更新/绘制对照；209240绘制、1255440原43ed45顶点与状态精确一致。原构造器、显隐、UV、颜色、几何和timer均执行原指令，只替代资源创建、GPU流锁定/提交、clock、投影和截图服务边界。51host/28Python/43CPUASan通过；普通/ASan Vulkan各320帧、8050206像素通道、160次capture边界，最大1/255。45实际入口开场交接+360phase1操作HUD帧，完整2927帧/1317140世界发布访问，PCM5619840样本普通/ASan同c39ec8b4443233cd。原始资源目录保持只读。

指定Mesa NVK交叉构建通过，NRO 1a4ed1b6a82210733a2b227072a7ef64cc92d3f08cd795b408da19b197f3cb63，undefined0。HUD尚未接可玩应用注册，因此链接裁剪下NRO相同；主机MoltenVK回读不等于Switch实机验证。完整移植未完成，SD归档仍0.3.5，防休眠继续。
