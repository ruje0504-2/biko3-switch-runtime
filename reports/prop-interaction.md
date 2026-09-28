# 道具交互与音频保位续播

`game/prop_interaction`实现完整4f4306。按16个present槽顺序处理，hidden不作为通用跳过条件，前槽改动的玩家action直接影响后槽。车辆0/13..17要求精确碰撞名、XZ50、水平速度非零、HUD门控0且非玩家slot13；首次设置+337 latch、sound stage4、原位置/沿车朝向100的目标及yaw+180单次回绕，随后每次设玩家slot10、completion_mode1、script_phase1。kind1/2精确匹配字面NULL且移动便写outcome1。kind3/4在XZ40与Y±5内非idle动作设stimulus1、action2/10并仅stage0时直接循环Play；离开设action0/8、stage0并直接Stop，近身idle保留旧状态。

kind5在XZ50、Y±5和未归一化的后向角差[-90,90]内，idle设置独立+854计时500且仅清armed，slot3等待原timer，其余动作直接outcome5/选中数组索引。kind8/9/12仅XZ20、Y±5、玩家slot3/8、道具action0时改action1、一次Play、stimulus2。kind10/11/18/19在XZ100、Y±5时复制上次发布root world、覆盖translation为prop position/Y+20；后槽覆盖前槽。每次只清可用字节，无匹配保留旧矩阵。

状态借用既有player control、prop motion/sound及共享NPC stimulus/outcome；新增每道具car latch和独立alarm timer。所有CPU拒绝原子提交，真实音频在CPU成功后依序执行，后端失败终止帧。`prop_assets_bind_interaction`提供真实root缓存与同一份逻辑状态；新实例car latch0，跨入口保留的alarm timer由最终场景重载控制器恢复（511940不重置此timer）。没有提前重新发布模型或在交互时推进动画。

`media/audio`增加pause/resume，保留clip、gain、frequency及分数采样相位；命令仍在下一未提交帧生效，已排队声音保持不可变，最新状态和可听游标分别查询。正在播放重复resume不倒带，最新loop参数替换旧值；自然结束的本地end-sentinel在resume时回到0。Stop保位及Play更新flags依据[Microsoft Stop](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418151(v=vs.85))、[Microsoft Play](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933(v=vs.85))。结束后重启及按队列边界生效是明确的本地缓冲策略，未作Windows设备时序比对。scene/prop_audio直接Play/Stop使用这些接口；已加载但未提交的buffer先绑定，首个Stop不产生任何PCM；后续空间音量与频率更新不会解除pause。

验证：

- 6,000完整原函数帧、35,806 present道具，原CRT trig/heading/strcmp/矩阵复制/计时均执行；仅DS Play/Stop及GetTickCount是服务边界。257首次车辆反应、180新增警报结果、2,299提示矩阵、232循环Play、44一次Play、2,103 Stop，全部状态与调用顺序一致，float最大误差0。
- 独立有理数音频时间线验证1,600次暂停/续播/变速/loop/声道切换，包含排队旧声音、重复命令、同边界Stop+Play、保留分数相位、一次结束及新clip替换；普通/ASan均通过。
- 45真实配置、62道具、744帧实际模型显示→交互→混音，26 Play、90 Stop、11非零位置续播、78提示。1,428,480 PCM采样与独立逐采样参考最大容差1，1,172,172非零，普通/ASan FNV64均632c64b222fa9399。检查交互没有改变本帧已经推进的XAN/缓存root；这是离线测试。
- 50主机CTest、28Python、42CPUASan通过；devkitPro不声明strnlen的问题已改为标准C有界检查，随后重跑相关主机/ASan、完整原交互oracle与Switch构建。
- NVK交叉构建undefined symbols0，Mesa 5dba7886c56460ff47c3038e323d07b9547d6212，NRO `4fd8c11107f8ff008bab18ca5b469d8b972846ec71112f3ad67e7c15966a5908`。应用仍为诊断场景，SD归档未更新，不能把这些证据当作完整可玩或Switch真机验证。

下一项中央51a682/4ec78d装配与phase0→1过渡。全部移植尚未完成，Mac防休眠继续。
