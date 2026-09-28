# model 模块（CPU 模型、材质、静态适配与点光环境已实现）

输入是资源层返回的只读字节，输出是拥有明确所有权的 CPU 模型资产：网格、索引、材质/纹理引用、节点层级；后续加入动画轨道、骨骼权重和变形。

只依赖 core/resource，不包含 Vulkan、libnx、游戏任务状态或存档。保留原版字段和单位，在证据充分后定义转换。格式未知或引用无效必须返回错误，不能创建“空模型成功”。CPU 模型释放与 GPU 缓存释放分开。

已加入 `bk_model` 构建目标：`model.h` 定义数据所有权和解码结果，`model.c` 实现 OBJM 的网格、材质数值、纹理引用、节点层级与世界矩阵组合。输出包含独立拥有的源数据副本，调用方可立即释放输入；用 `bk_model_destroy` 统一释放。对象视为不可变，禁止在解码后改写计数或指针。

当前返回三类结果：`BK_MODEL_OK`、`BK_MODEL_INVALID`、`BK_MODEL_UNSUPPORTED`。DirectX 文本 `.x` 和未知 OBJM 头版本明确返回不支持。MESH/MATE/TEXT/FRAM 以外的 chunk 保留原始数据，由明确的适配器进一步解析；保留字节不表示已经支持动画、变形或特效。

已验证 247 个 OBJM 的完整 CPU 数据；3 个 DirectX 文本模型尚无解析器。此模块没有绑定 renderer，Switch 交叉编译生成 `libbk_model.a`；0.3.0 由办公室场景调用该模块并转换为 GPU 数据。

`material.h` / `material.c` 还原 alpha 编码、混合方式、可见性、缓存提示和高光开关；不把 alpha 缓存提示当成 GPU 关闭混合的命令。纹理加载通过明确的 resource pack 返回 CPU 图片，TGA 标志遵循原版文件名分类，缺失/损坏不退回默认图片。模型材质不决定深度写入、光照、雾或透明排序。

`static_model.h` / `static_model.c` 在已解码、不可变的模型上生成基础姿态实例与世界矩阵，根据原版队列规则明确指定深度写入，并执行距离/优先级排序。该适配器借用模型，自身销毁不释放模型；它仍只依赖 CPU 数据，不创建 GPU 对象。特殊拓扑/状态、多纹理和超过 4096 实例明确拒绝。

`environment.h` / `environment.c` 在不可变模型上解析 LIGH/FOG，将灯光 ID 与 FRAM 关联，复制 CPU 环境数据并产生 core 的 `BkLighting`。点光位置使用关联节点的世界原点，环境光保留原版 8 位量化。当前接受至多一个环境光、八个点光；无关联/重复关联、其他灯光类型、启用的雾及高光材质明确拒绝。环境对象独立持有副本，用 `bk_model_environment_destroy` 释放；上传灯光 uniform 仍由 scene/render 处理。

格式证据与历史阶段见 `reports/model-cpu.md`、`reports/materials.md`、`reports/static-scene.md`；0.3.1 光照与限制见 `reports/lighting.md`。办公室已绘制，M1 整体尚未验收，下一步核对原版相机/时间绑定和完整画面对照。

`animation.h` / `animation.c` 已加入相机 ANIM 的完整 SRT 轨道采样；220 字节键中的位置/缩放线性插值、四元数最短弧及原版整数循环已对照。该适配器借用不可变模型，拥有解码轨道，输出独立世界姿态；`bk_model_pose_world_matrices` 提供外部局部姿态的层级组合。不修改原模型，不包含 GPU、片段调度或角色/流程状态。0.3.5 已加入稀疏通道补齐与非单位四元数；曲线位置和旧布局仍明确拒绝。0.3.3 证据保留于 `reports/camera-animation.md`。

`clip.h` / `clip.c` 在 0.3.4 加入相机 XAN 片段配置和独立播放器，输出单点或 SRT 过渡采样请求。配置与运行状态分离，不使用磁盘指针；普通播放器忽略旧计时值，0.3.5 的显式 authored 构造保留校验后的原资产播放标量。`animation` 新增双端 SRT 采样接口，位置按原 smoothstep 缓动。场景负责选择片段并将采样请求交给动画对象。当前证据和限制见 `reports/camera-clips.md`；未包含角色装配、碰撞或游戏状态机。

`playback.h` / `playback.c` 在 0.3.5 将时间轴推进、动画采样和可选根放置组合为原子提交。对象借用模型与 XAN 配置，拥有解码动画、播放状态与双缓冲姿态；先销毁实例再销毁资产。独立实例不修改共享模型。原版重复动作请求、实际头节点及原 D3DX 姿态遍历的证据见 `reports/actor-pose.md`。该能力不包含蒙皮、渲染或游戏帧阶段选择。

后续 `clip` 已支持0x4018c8保留当前片段过渡长度的请求，和0x401b0a固定10 tick请求分别处理。两者均比较requested，重复请求不重启；configured长度可能已被此前请求改写，不能每次从磁盘默认值取回。

`playback` 已恢复动画组缓存：0x409660初始化普通采样缓存为0；0x4097d6遇同时间跳过，0x409a94混合采样不改这个时间。因此相同普通时间可能保持最新混合姿态。根变化仍重组世界矩阵；失败不提交时间、缓存、姿态。无状态 `animation` 采样接口本身仍每次计算。见 `reports/movement-animation.md`。

文本相机姿态已有单独`x_pose.h`显式入口，供菜单双轨道使用。它只提取帧与SRT轨道，不解析网格/材质；通用`bk_model_decode`继续拒绝文本绘制模型。详见`reports/menu-camera-assets.md`。
