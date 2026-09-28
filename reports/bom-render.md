# GPU 蒙皮后的顶点回调

本阶段补齐 `scene/bom_render` 与 Vulkan 顶点传递，不改变已冻结的雨雪/原标题交付包。实际 flow16 仍未装配，不能将这项依赖验收称为结局已可玩。

## 所有权与帧顺序

`bom_assets` 提供只读 VIX、最近点映射和实际两个演员；`bom_render` 借用它及两个 `actor_render`，拥有每个绑定的 GPU 索引/描述符，并在两个演员安装同一所有者的网格回调。销毁顺序为 BOM render → actor render → BOM assets → actor pose/model。通用 `render` 接口只认识两份顶点缓冲、索引对和矩阵，不依赖游戏/模型模块。

两个演员均完成 prepare 后，BOM prepare 锁存源节点的已发布 world、原始 disabled 值和演员版本。辅助模型通过全模型 MORP prepare 上传实例顶点/材质；不改变姿态或发布缓存。后续重新 prepare 任一演员使旧 BOM 快照失败。

原 `42afbb` 执行 ENVL 后进入绘制队列；`444bc1/444bea` 在隐藏判断之后、材质 alpha 早退之前调用 `4aaebb`。当前实现对应这两个阶段：begin 先提交 GPU ENVL，每次真实目标网格绘制再执行所属组的绑定。仅 `disabled==1` 禁用，保留绑定顺序、重复网格提交和 alpha0 回调；隐藏节点不产生回调。相关原指令/CPU证据沿用 `bom-deformation.md` 与 `bom-assets.md`，本次未重复运行旧的逐指令归档。

## GPU 实现

`transfer.comp` 只修改 lit 顶点的位置和法线 XYZ（22 float 步长中的 0..2、9..11）。源位置按 world 变换并保留原近单位 W 判断，法线乘 3×3、不归一化，其他属性保持。不同源/目标缓冲的重复目标索引在创建时压缩到最后一次写入；同一缓冲保留全部顺序，由一个 invocation 串行处理读写依赖。跨绑定严格依次 dispatch。

回调结束当前 render pass，在 compute/vertex-input 间建立依赖，再用 LOAD 恢复颜色、深度、视口和 scissor。深度依赖覆盖 early/late fragment tests；回调不做 CPU 等待、GPU 回读、动态分配或逐顶点 CPU 计算。普通路径仍是 GPU 蒙皮；仅原有显式 `BK_CPU_SKINNING` 诊断开关可以选择 CPU 路径。同步范围参考 [Khronos 同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)，计算命令位于 render pass 外，遵循 [vkCmdDispatch 契约](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDispatch.html)。

姿态不变也必须恢复曾被回调覆盖的蒙皮目标。world prepare 请求下一次 begin 重算；GPU 回调另标记下一次纯重绘需要重算，当前帧内多次回调不重复蒙皮。这个“下一次重绘待刷新”状态与新的 CPU 输入待上传区分，诊断回读仍能读到刚完成的回调结果。释放回调时，仍被演员持有的蒙皮目标也会在下次 begin 恢复。刚性缓冲保持当前内容，显式 mesh update 才替换；一般投影矩阵选中顶点的 W 非零、输出有限是调用方前提，不宣称 GPU 非法值的事务回滚。

## 已执行验证

- 通用 GPU 对照：64 组双次有序回调、16 帧冻结姿态/开关、16 帧颜色与深度/子视口保留、3 次复用快照纯重绘，628276 个 float 检查。含重复索引、同缓冲别名、跨缓冲循环、近单位/一般投影 W、非法参数拒绝及调用方先释放网格后的保留所有权；最大归一化误差 `2.38419e-7`，其余属性逐位一致。冻结姿态16帧新增上传字节为0，目标蒙皮16次。
- 七组真实资源：504 帧、2580264 顶点，对照已验证的 CPU ENVL/BOM；最大归一化误差 `2.38419e-7`。42帧 alpha0、42帧隐藏、168帧重复演员提交；隐藏辅助模型仍保留已采样源顶点。所有非空绑定目标均为 ENVL，6241 个选中索引均受骨骼影响。最近点映射与现有 owner 逐项一致，零选择保持无 dispatch。过期快照、回调所有者冲突及错误 flags 长度明确拒绝。
- 两项新 GPU 探针均普通/ASan通过且输出一致；帧内记录的 GPU buffer/image 分配数量和字节无增长，每组释放回到基线。ASan 使用 `detect_leaks=0`，不冒充 LeakSanitizer 验证。
- ENVL 回归：131模型、687网格、1048帧、2206936顶点普通/ASan通过，最大位置误差 `5.36441803e-7`、法线 `2.38418579e-7`。MATA 的420帧/80640通道回归保留 FNV `d51b58261b830ce5`；MATA检查发生在后续重绘刷新修改之前，最终真实BOM与应用检查覆盖修改后的渲染器。
- 最终原标题→手柄选人→第五人剧情→雨天开场→两张照片→暂停恢复→原标题，普通/ASan一致：212标题、253选择、296剧情、195加载、492开场、106暂停、111返回标题帧。最终RGBA摘要见 JSON。
- 五项相关普通/ASan单元、两项模块架构检查通过。指定 Mesa NVK 交叉构建通过，未定义符号0；构建摘要在 `bom-render-verification.json`。这是构建和主机 MoltenVK 证据，没有新增 Switch GPU 实测。

## 范围

七组模型通过 BOM metadata 选取仅是明确的资源夹具；生产完整入口必须继续按原版表选择。测试只读数值缓冲，真实模型裁剪到画面外，不声称原版完整帧视觉对照。新的回调不等同于每帧 BOM 节点控制/物理、相邻 `4a52bc` 装配或完整结局逻辑；这些仍待恢复。按秒 MORP 组过渡 `433cbe`、实际主角/辅助生命周期和 flow16 也未完成。全移植目标保持进行中，Mac 防休眠不解除。
