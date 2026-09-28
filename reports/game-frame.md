# 中央51a682/4ec78d更新装配

新增game/frame_dispatch：先追加旧发布world的动态碰撞，再复制phase到NPC motion.mode并锁定本帧分支。phase1按背景→玩家控制→玩家镜头→玩家表现→NPC空间/事件/脚步/阴影→NPC表现→道具空间/声音→道具表现→物品表现→道具交互→发现结算→区域边界→拾取执行。phase0/2使用idle玩家，物品表现后分派跟随/交接镜头。其他phase只追加/清理碰撞和复制mode。4ef141仅为空循环。后续检查纠正：4cc320实际将HUD reserve(709c64)恢复为1，由拥有HUD状态的应用在锁存phase0/2的更新尾部执行；此前本报告误称空函数。

scene/game_frame实际调用已实现的CPU资源和音频服务。绑定共享刺激/玩家noise、709084锁存、RNG、区域ambient gate、当前真实background clip、保留玩家action表和NPC动作表；道具触发读当前实例，区域写回同一实例。保留外部初始化/存档状态，不在update内猜new-game默认值；没有孩子world提前发布。调用者在整个update前poll音频、后fill，绘制阶段才发布全局森林。运行失败中止后续阶段并清理碰撞后缀，保留首个错误，不允许重试半变更的会话。

验证：

- 原51a682/4ec78d指令2400次，17674事件，覆盖所有256个原phase、服务内变更phase、不同镜头stage/transition；分支锁定、NPC mode写入时点、调用顺序一致。68种portable失败路径检验清理/首错误保留。此oracle替换组件体为观察服务，不声称完整Windows游戏帧数值等价。
- 45组真实entry/background/prop/item，1080组合帧、2208动态mesh追加、512722森林发布访问、2073600PCM样本。普通/ASan完全相同hashb28d69fe1e966e4a、1967174非零样本。phase0/2/1/3为显式fixture，540次follow障碍miss和固定投影输入为明确fixture，未假装51a190已完成流程切换。每组均验证缺失必需障碍服务失败后清理后缀。
- 50host、28Python、42CPUASan通过，Switch NVK交叉构建undefined0，NRO 869f79e569b898630d5820f093125357bb4caa57a072933fe8f037121a7b8448，应用尚未引用新中央帧，dead-strip使NRO不变。

当前follow障碍服务为必需注入依赖，真实4bef54/4bed86/4b61e5待恢复；其结果会缩短+43c，应集成到预平滑后、修正平滑前，不能仅在调用前给固定correction。初始化/存档、菜单键、原投影输入和51a190对话/UI、主渲染/应用注册也仍待接入。报告不代表完整可玩或Switch真机验证。SD仍0.3.5，继续下一依赖，防休眠保持。
