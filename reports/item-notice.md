# 拾取提示面板、图标与计时

`ui/fade_sprite`恢复50e633/50e6ba中enter1/exit1/idle0且无secondary的明确子集。初始化alpha1、stage0；hidden更新置alpha0。请求只在stage0且值1、stage3且值0时生效，过渡中不反转。进入每帧alpha+=speed*.5*seconds，到1进入stage2，下一帧才stage3；退出先从alpha1起步，同帧递减，到0才stage0。原x87中间值在最终alpha写入前不提前舍入。其他动画模式未宣称完成；非有限、负速度/时间、越界alpha/stage明确拒绝。

`scene/item_notice_state`组合原51a190 phase1的notice段。共享`BkItemPickupState.notice_timer_armed`（旧名notice_phase已修正），它实际是beeb88+144即beeccc，真正sprite阶段在+134。visible恰为1才轮询5000ms原计时器；到期帧仍绑定message并重置font started0/enabled1。随后panel、五个icons按原次序请求/更新，仅selected对应icon请求visible；更新后的panel stage2/3才绘字。deadline/armed低32位有符号比较保持原行为。CPU阶段无效参数原子保留；初始化保留deadline，按BEF778保留旧icons状态。

`scene/item_notice_render`拥有bk3_00/ma_04.tga、五组共25个配置图标、Type_S.FTT文字组件及GPU资源。图标文件来自原4e82b8，面板138/727/1010/208、图标192/768/128/128乘游戏视口宽/1280。六次精灵提交后才有条件提交原字体多遍绘制；443daa的顶点alpha先截断到8位，D3D整数像素中心转换为Vulkan半像素。消息绑定借用活对象，保留可变消息指针；最初显式空消息。重新加载可保留旧组图标，资源失败保留旧资源。该组件只负责phase1拾取提示，其他phase对话/完整HUD与全局字体共享装配仍待接入。GPU/字体失败终止帧，不声称撤销之前CPU状态。

验证：

- 原51a190 notice段18,960帧：9,434次文字绑定、7,161次文字绘制、805次计时到期，timer/六sprite/font门控状态一致。执行原计时器、状态机；只替换rain/其他HUD与字体/绘图边界，段后其他UI未执行。
- 原4e82b8至4e8bcf加载段，25组分辨率/人物配置，完整50de40、43ed45和443daa：1,200精灵帧、7,200顶点及量化颜色误差0；125个图标在禁止重载时完整保留。贴图/顶点锁定与绘制为边界。
- 实际拾取服务接音效/文字再接提示：75次拾取、310Vulkan帧、10,272,000像素样本，900,207个样本区别底色；普通和ASan均最大1/255。含中途换图标、到期、淡出中再次拾取、宽屏内4:3子视口；10帧额外全缓冲逐字节检查保留资源与失败重载，已检查生成画面。离线声音服务使用显式sink，本阶段不冒充可听输出或Windows完整帧对比。
- 单元覆盖到期与stage2的交错、淡出中拾取、保留图标、后续非法sprite导致整段原子拒绝。49主机CTest、28Python、41CPUASan通过。
- 指定Mesa26.2.2/NVK构建成功、undefined symbols0；NRO SHA256 `687b520d254877dad7352750708c34c184b38a2d179c7d2222f45cb7db7ba0ca`。新提示组件仍未接应用完整游戏流程，NRO变化不能当作可玩证明。SD仍0.3.5，不发布新完整移植包。

下一项回到51a682中央帧，恢复4f4306道具交互、4f3c80发现结算、4f3d34边界/区域声音与完整调用顺序。完整移植及Switch实机验证未完成，Mac防休眠保持。
