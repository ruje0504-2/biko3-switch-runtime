# 角色入口状态与保留字段

实现 game/actor_entry，连接 entry_assets 与 game_frame_initialize_actors。玩家实际出生点遵循4befc0的flow48覆盖；entry selection仍表示正常出生表，NPC高度仍从该表取。玩家4bf1a0初始vertical是预放置头Y+10，NPC是预放置头Y+正常出生表Y；不会提前发布子节点。清速度/pitch/interaction_mode、surface/wall/heading、trigger和completion；动作槽7/15、previous/turn/acceleration、script_phase/return_yaw、墙面传感器与全局latch保留。flow8/38清已映射5字节库存，group1强制库存1为1；道具创建必须在此之后。

NPC4fad20恢复action1、behavior1、alpha1、action_wait.duration2000与surface/background_wait/fade，保留hidden/stimulus/head、run_remaining和其他timer字段。未写的footstep walk[1]/run[1]保留，不再在中央循环夹具中假定2/5。Actor+834/+838单列为npc_entry_route，不与插值segment_start/end混淆；起点来自明确传入的BF3EA8状态。先读取起点flag，再对flow48当前点写1，包含起点=当前点的情况。route只允许修改活动点，禁止污染哨兵或制造提前终止点。

scene在新鲜entry上仅执行一次演员CPU初始化、已有face warm-up和4ebfd0的phase写入，沿用RNG/时钟；保留其他session/UI状态。玩家+7c8和interaction.outcome是同一71bcd8字节，中央循环现于控制边界和帧返回同步；NPC的71bcdf/e0直接读库存3/4，移除外部重复输入。入口重置同时清对应response/outcome。还未实现完整外层loader、新游戏/存档状态来源、对话/51a190或可玩应用。

验证：

- 原4befc0完整函数、4bf1d4..4bf3e8、4bf5c4..4bf5f4：11520玩家组，45profile×全部256flow字节，非零旧状态/动作槽/库存，最大误差0。
- 原4fb5f0..4fba03含真实4ff78e CKP读取、原setter/heading：2768NPC组，全部45route，248组先读后写同一点flag，最大误差0。112个直接向NPC初始化器传入的无效flow48游标单列拒绝；这不是实际外层入口分派，外层4e82b8会先把区域改为4/5/5/6/5。5539无效输入/哨兵修改原子拒绝。
- 实际模型/头/根/face初始化166组，14个直接无效组合拒绝；保留计时器/动作槽/latch/旧UI字段、库存先于物品、重复初始化失败均验证。
- 中央循环45真实入口1080帧、2208动态mesh、512722发布访问、2073600PCM采样/1967138非零，普通/ASan一致hash201816ecf775411f。相位切换/投影/初始route_start与全零保留区仍为明确测试输入，未冒充原新游戏。
- 50host、28Python、42CPUASan及Mesa NVK交叉构建通过，undefined0，NRO 323ebf443fd698482c21e113097d792bde5d32ac90f7c9c12ed9a0c695b03221。

下一项外层4e82b8区域/资源进入流程、真正启动状态来源，再51a190对话与阶段切换。SD归档仍0.3.5；未验证Switch实机。Mac防休眠保持。
