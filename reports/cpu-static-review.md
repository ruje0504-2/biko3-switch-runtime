# 2026-09-28 CPU 热路径静态审查

审查范围为现有可运行开发版的每帧路径：application → play_session/game_preview → 中央游戏帧 → actor_forest/actor_render → Vulkan，以及独立音频线程。以源码和 Apple M4 的主机采样判断候选热点；没有本次 Switch 的 CPU 周期或各核采样，不能由静态审查断言某一项就是 53 FPS 的唯一原因。完整移植仍在进行。

## 已修复并纳入本轮更新

| 问题 | 原实现的额外工作 | 本轮处理 |
|---|---|---|
| CPU ENVL 蒙皮、转换、上传 | 每帧逐影响变换位置/法线、转换整个网格并上传；主机采样确实落在 skin_apply、mesh_update、actor_render_prepare | 预处理不可变权重顺序及 beta 阈值；Vulkan compute 蒙皮，每帧主要更新骨骼矩阵。旧 CPU 路径仅由 BK_CPU_SKINNING=1 显式启用供对照 |
| 重复绘制排序 | 每个模型先排序，游戏随后从原始遍历重新组全局队列并再次排序；普通队列双循环为 O(n²) | 单模型排序延迟到真正单独绘制；全局普通纹理键不变时复用原交换序列的置换。透明距离/优先级仍逐帧计算，不更换原版排序算法 |
| 世界矩阵重复乘法 | actor_forest.compose 已计算并校验 local×parent；commit 再调用 publish_node 重算一遍 | 全遍历预检成功后，world 内部接口提交已算好的 world 和 parent cache；保留隐藏、换父节点及失败原子性 |
| 背景灯光每帧堆分配 | prepare_render 的 world_copy malloc、逐节点 memcpy，然后立刻 free | 只读借用同一时点的已发布矩阵；生命周期限于该次调用 |
| 空动态碰撞仍重建静态表 | 无有效动态物体时也分配临时 Collision、静态网格描述表和 kinds，并复制整个静态前缀 | 无候选直接清理旧动态后缀；有效候选最终没有网格也复用静态前缀。活动动态物体原构造规则不变 |

源码入口：`runtime/scene/actor_render.c`、`runtime/scene/skin_upload.c`、`runtime/render/shaders/skin.comp`、`runtime/model/draw_order.c`、`runtime/world/actor_forest.c`、`runtime/world/actor_pose_internal.h`、`runtime/app/game_preview.c`、`runtime/world/collision.c`。

## 仍值得优化的具体实现

1. **动画采样的临时内存与重复世界姿态。** `model/animation.c:pose` 每次分配 local/result 两个矩阵数组，`model/model.c:bk_model_pose_world_matrices_under` 再分配 state/stack。`model/playback.c` 本身已经持有 next_world/next_local，仍经过这些临时数组。放置或保持时间也可能进入这条路径。优先考虑实例独占工作区、加载时建立遍历顺序和局部姿态专用采样接口。不能直接删除 playback.world：诊断和独立 actor 发布仍消费它，根/眼/相机也有旧缓存语义。收益尚未独立量化。
2. **碰撞的重复全表扫描。** `game/player_scene.c` 每帧两遍墙面遍历，随后扫地面；`game/npc_scene.c` 扫视线与地面；`world/follow_obstacle.c` 又扫镜头障碍，均无空间索引。NPC 已不可见时仍继续进行后续视线交叉计算。主机采样出现玩家空间/线段相交热点。可预计算静态三角形分类、范围和空间索引，用保守筛选减少窄相查询；必须保持候选原序、玩家在查询中被逐步推开后的覆盖范围，以及共线/退化边界政策。不能随意排序三角形或只按帧初位置筛选。
3. **逐网格重复提交 Vulkan 状态。** `render/vulkan/renderer.c:draw_mesh` 每次绑定 pipeline、纹理和灯光 descriptor，再推送矩阵，即使相邻网格状态相同。应缓存 command buffer 内的已绑定状态，截图结束/恢复 render pass 时正确失效；再评估合批。不能合并会改变透明、光照组或截图边界的绘制。需要 NVK 实测其驱动 CPU 成本；主机 MoltenVK 的比例不能替代。
4. **单帧资源与提前 fence 等待。** 同一组可写缓冲/灯光 uniform 迫使下一帧 CPU 更新前等上一提交。`wait_frame` 在同一提交后实际上只等一次，后续调用检查 submitted=0 即返回，不能把调用次数算成多次 GPU 阻塞。可通过多帧独立 palette、uniform、动态顶点区提高 CPU/GPU 重叠，涉及截图、销毁和纹理更新生命周期，需单独实施。Fence 等待是依赖等待，不是等量 CPU 运算。
5. **灯光 uniform 重复刷新。** 同一次 prepare 分别 update/view/fog，各自复制与 flush，基础灯光不变也重打包。可合并脏范围和提交时机。数据量比旧逐顶点上传小，优先级低于上述项。
6. **音频锁范围。** `media/audio.c:bk_audio_fill` 持同一锁覆盖混音和设备提交；主线程 cursor/gain 可能等待。声音已经在独立线程运行；继续加线程无直接收益。若 Switch 日志证明锁等待明显，再将不可变混音快照与命令提交拆开，保留声音版本、暂停分数游标及口型消费时钟。当前没有证据证明这是本轮主要掉帧源。

## GPU 与多线程的选择

已将大批独立顶点的 ENVL 计算交给 GPU；原逐顶点灯光本就运行在 shader。脸部 MORP、材质顶点常量拆分也可后续迁移，但当前上传已大幅缩小，应重新采样后排序优先级。

玩家/NPC 决策、碰撞和镜头需要当帧返回数据，且读取阶段不同的旧/新发布缓存；直接搬到 GPU 会引入读回或改变时序。骨骼 SRT 将来可以按独立实例并行，但应在明确的阶段屏障下提交，避免主线程等待任务、音频抢占和共享 RNG 改序。先去掉重复工作，再评估工作线程的净收益。

总 CPU 利用率和 GPU 低占用不能单独排除 CPU 临界路径或 CPU/GPU 同步限制。本轮 PERF 新增 skin dispatch/vertex 数和 cpu_work：后者是帧墙钟减去已记录 fence/acquire/present 的估计，仍包含系统调度、锁和其他等待，不是硬件 CPU 周期计数。

具体构建、回放、误差与前后测量见 `gpu-skin-performance.md` 及 `gpu-skin-performance-verification.json`。下一轮优先处理动画工作区与碰撞候选筛选，随后验证 Vulkan 状态缓存/多帧缓冲；不以无依据增加线程数代替分析。
