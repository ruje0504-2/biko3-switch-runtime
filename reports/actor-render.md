# 动态角色绘制与应用接入

当前工作树新增真实角色的Vulkan动态绘制，主机`--scene actor`、Switch Y进入明确标识的角色资产检查场景。它连接已验证的动作/SRT、ENVL蒙皮、FAM/MORP和材质实例。办公室仍为静态预览；完整游戏流程、音频、玩家控制/碰撞和实机验收未完成。SD归档仍为0.3.5，未用新NRO替换旧包。

## 实现

- `render`的lit/unlit网格支持固定数量顶点更新。先完整检查有限值、数量、格式和所有者，再等待上一GPU提交，复制映射内存并flush；拒绝活动帧内写入。无逐帧GPU分配。flush错误带VkResult上报。
- `scene/actor_render`拥有纹理、GPU网格、CPU蒙皮实例和复用暂存区。prepare在begin之前读取已经发布的姿态；不推进AI、时间或角色发布。ENVL输出用单位world，刚性和面部MORP部分用所属节点world。当前材质alpha影响队列和深度写入，隐藏父节点抑制子树入队；零alpha材质仍保留排序位置，绘制时才跳过。
- 材质高光和灯光specular接通，观察者位置可在帧前安全更新。高光逐顶点计算、单独插值，在纹理乘色之后、混合之前加入。原版`0x42967f..0x4296da`设置LOCALVIEWER=1、SPECULARENABLE=1、NORMALIZENORMALS=1；材质power>=.01的开关沿用已有原版对照。
- 原始刚性附件存在world[15]不等于1，例如h01_80相关节点约.999999702，克里斯附件约.999777。保留原始齐次裁剪数据，光照位置使用`world.xyz + eye*(1-world.w)`，等价于刚性视图中的原始world*view XYZ；不提前做透视除法或修改资源矩阵。仍拒绝投影型world和奇异线性变换。
- `model/triangles`支持header+56为0/1/2：索引三角列表、非索引条带、非索引扇面。条带/扇面使用完整顶点序列，忽略文件索引；条带每个奇数三角形反转前两点，保留退化三角形。克里斯的四套服装各含条带，之前静态适配器明确拒绝这些部分。
- actor检查场景借用真实entry(0,8)资源，半速推进当前动作，固定静音口型输入0并推进眨眼；按已验证的可见性规则隐藏标记节点。摄像机与灯光为检查设置，未代替原版游戏相机/选灯。应用新增bk3_01及routes/faces挂载，完整构造后切换和失败回收沿用原流程。

高光数学参考微软[Specular Lighting](https://learn.microsoft.com/en-us/windows/win32/direct3d9/specular-lighting)，混合顺序参考[Render States](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype)。这是固定管线规则实现及独立CPU公式/GPU回读对照，**没有执行Windows原版光栅器，不能声称原版像素一致**。观察者位于顶点、半角为零或零法线等退化情况定义为无高光。

## 已执行验证

所有原地址仅适用于中文EXE SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

| 检查 | 结果与边界 |
| --- | --- |
| 动态缓冲GPU探针 | 16帧位置/颜色交替，lit和unlit均覆盖；上一帧提交后立即修改再回读，旧帧保持；数量/所有者/格式/末顶点NaN/活动帧拒绝 |
| 光照GPU探针 | 22组双精度独立参考，误差<=2/255；高光指数/阈值、黑纹理、高光饱和、非均匀世界变换、非单位常量W、观察者更新、范围、背光、零法线及UI状态恢复 |
| 原D3D网格提交 | 21模型加合成边界，共845次调用选择与参数检查，转换483073三角形；原条带/扇面走DrawPrimitiveVB，列表走DrawIndexedPrimitiveVB |
| 原灯光提交回归 | 71灯光/节点变换和128环境色量化检查通过；不覆盖完整游戏的逐遍选灯 |
| 实际角色GPU集成 | 45入口配置，每组12帧，共540动画回读；另135次隐藏/零alpha/失败后恢复回读。图像确有变化且正常帧非空；CPU蒙皮/表情数值证据沿用此前报告 |
| GPU sanitizer | 同样45入口、540动画+135状态回读；另有光照、动态缓冲和场景生命周期的ASan/UBSan验证 |
| 应用场景 | 主机actor启动成功；3轮actor/office切换、每轮120固定步、draw不推进模拟、缺资源失败清理通过；title/office/camera-track原生命周期回归通过 |
| 自动检查 | Host CTest 25/25、Python 23、CPU ASan/UBSan 19/19 |
| Switch | Mesa NVK固定提交交叉编译，ELF未解析符号0；实际硬件未验证 |

新NRO的SHA-256为`ac5d8626b9a78f70e23b687489ec09c907a2ce9c6654230abc74b343f84f1b74`，构建版本仍0.3.5工作树；依赖仍Mesa26.2.2 / `5dba7886c56460ff47c3038e323d07b9547d6212`。源文件、shader、日志、oracle、二进制与回读摘要见`actor-render-verification.json`。

## 接续工作

角色检查场景证明CPU变形进入应用及NVK链接，尚不包含眼纹理切换、眼球朝向、附属物完整生命周期、4fc36d完整表现阶段、4fd796动作/脚步声、玩家墙面移动、主流程和音视频/存档。下步继续恢复眼部资源与完整NPC帧阶段，再把已有CPU策略装配进真正游戏场景。M1未整体验收，不恢复Mac休眠、不将阶段检查交给用户。
