# M1.3：原版办公室静态接入

0.3.0 已在主机 Vulkan 上绘制原版 `bk3_03.pp/m01_04.x`，并生成链接指定 Mesa NVK 的 Switch NRO。当前使用 **FRAM 基础姿态、无光照/雾的纹理材质和开发检查相机**。没有角色动画、碰撞或玩法；尚未进行同视角 Windows 原版截图对照和 Switch 实机验证。M1 整体仍未验收。

## 数据与工程实现

办公室包含 38 个网格、66 个子网格、64 个材质、42 张纹理、65 个节点。节点对网格的复用展开为 93 个实例，其中首队列 63 个、排序队列 30 个。4 个子网格依照原版零 alpha 规则隐藏；其余 62 份 GPU 网格共上传 7,657 个顶点、19,572 个索引。42 张 RGBA 纹理合计 6,060,032 字节。队列计数包含隐藏项，以免删除项改变原版交换排序的次序；真正绘制前再检查可见性。

- `core/matrix` 统一行向量矩阵、左手视图和 Vulkan 深度/Y 适配。
- `model/static_model` 只处理 CPU 数据：世界矩阵、基础姿态实例、材质队列与排序。借用的模型必须保持不可变并比适配器存活更久。
- `scene/static_world` 通过 resource store 加载办公室，将可见子网格上传一次、按实例复用；材质缺失、图片缺失或未支持状态明确失败。
- `scene/inspection_camera` 是可单独验证的开发相机，供场景检查使用，不宣称恢复原版控制。默认位置 `(-48,24,-45)`，yaw `0.830`、pitch `-0.152` 弧度，视场角 60°、近远裁剪面 0.2/2200。
- `render/vulkan` 为网格增加明确剔除状态；原有混合、深度写入、索引、repeat 采样接口保持分层，标题 UI 使用不剔除管线。
- `app` 在帧间创建下一场景，成功后回收旧场景并清空输入/计时积压；失败显示诊断并清理，场景不会伪造成功。

本适配器明确拒绝非默认 UV 绘制模式、非 triangle-list 拓扑、特殊状态覆盖、多纹理重绘和缺失材质/纹理引用；实例总数限制为 4096。它不是全部 247 个 OBJM 的通用绘制器。办公室恰好落在已支持的静态路径；LIGH、FOG、ANIM 等块保留在原模型中，但尚未用于绘制。

## 原版证据和适用范围

地址仅适用于中文 EXE SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。程序读取该文件并在 Unicorn 中隔离执行选定指令，不启动 Windows 游戏或操作系统接口。

| 地址 | 当前核对内容 |
| --- | --- |
| `0x418734` / `0x418749` | 子网格原始头 `+76` / `+80` 对应运行时优先级 `+f4` / 无符号距离偏移 `+f8` |
| `0x4228de..0x4229ce` | 单网格队列分类；材质编码 alpha 严格位于 float32 的 `0.999999` 与 `1.000001` 之间时视为近 1 |
| `0x422a17..` | 组中子网格采用相同分类，并标记已入队状态 |
| `0x42a65f` / `0x42b582` | 首队列 / 排序队列提交；调用参数包含深度写入开关 |
| `0x42c3fc` | 排序准备：取相机空间的节点原点长度，加子网格无符号距离偏移 |
| `0x42c56a..0x42cbb9` | 两遍交换排序：距离降序，再按优先级降序。相同键的顺序不能用稳定字典序排序替代 |
| `0x42b934` | 逐项设置深度写入、世界矩阵并绘制，队列结束恢复深度写入 |
| `0x42949c` | 初始化 CullMode=3，即 D3DCULL_CCW |

分类规则：有纹理且编码 alpha 不在近 1 区间时，进入排序队列并关闭深度写入；近 1 且首纹理有原版 TGA alpha 标志时，进入排序队列、保持深度写入；其他已支持情况进入首队列并保持深度写入。无纹理子网格走首队列。该阈值与 M1.2 材质混合分类的阈值不同，不能共用一个近似判断。

`tests/original_static_oracle.py` 对原版材质 setter 后的单网格/组子项分支做 **1,324 次** 检查（合成/阈值 1,192 次，办公室原材质 132 次）；对含重复距离、重复优先级及无符号大优先级的 **102 组** 排序做指令级对照，全部通过。

这些检查只覆盖队列分支与已给定距离键的排序。当前按文件中兄弟节点的顺序深度优先生成实例；尚未对原版完整遍历、动态隐藏标志、距离计算浮点舍入、灯光/雾变化和整帧提交逐项回放。因此不能把上述数字解释为完整原版渲染一致性。

## 矩阵、方向与 GPU 验证

保留原始顶点、UV、索引与坐标单位，CPU 使用 `world = local * parent`、`MVP = world * view * projection`。行主序/行向量矩阵字节作为 GLSL 列主序矩阵读取，等效转置，不再手动转置。投影矩阵只在 Y 轴统一翻转，Z 映射 `[0,1]`；正高度 viewport 下使用 `frontFace=CLOCKWISE` 与背面剔除，对应原版剔除屏幕逆时针面。枚举含义参见 [Microsoft D3DCULL](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcull) 与 [Khronos VkFrontFace](https://docs.vulkan.org/refpages/latest/refpages/source/VkFrontFace.html)。

`test-static` 核对非交换 W/V/P 顺序、别名乘法、相机原点、投影 Y/近远深度、非法矩阵和检查相机的死区、速度、固定步、俯仰限制及重置。`scene-probe` 在真正 Vulkan 管线上回读顺/逆绕序、有/无剔除、近/远面外裁剪以及 UV 两角采样，6 组像素检查全部通过。

原版办公室集成验证使用 Apple M4 / MoltenVK 1.4.2：

- 基础视角连续 16 帧 RGBA 完全一致；检查截图可见办公室桌椅、书架、门窗、地毯及贴图，未出现全局镜像或颠倒。
- 更新检查相机 60 步后画面改变，按 A 重置后逐字节恢复基础视角。
- 三轮办公室→标题→办公室，先构造新场景再销毁旧场景；回到办公室后逐字节一致。
- 新场景缺少资源时构造失败，已有场景仍可正常绘制；应用层选择报错并退出，不将它映射为标题或空白成功。
- 标题回读与 M0/M1.2 基线完全一致；应用办公室入口与生命周期探针生成的办公室画面也完全一致。
- 主机 CTest **8/8**、Python **20/20**；启用 ASan/UBSan 的 CTest **8/8**，原版办公室完整加载、绘制和上述生命周期检查均通过。

无光照着色器为纹理乘材质 diffuse；画面中素材自身带有的亮暗、光斑、透明叠加不能作为原版灯光已恢复的证据。

## 构建、打包与复现

Switch 0.3.0 最终 ELF 未解析符号为零，NRO SHA-256 为 `29df7daa3ce1d947da26d3753ff20295f8497497ceabbc7299d31faf09313731`。继续使用锁定的 Mesa 26.2.2 / `5dba7886c56460ff47c3038e323d07b9547d6212`，以及匹配的 Rust nightly；没有更换渲染后端。此结论只代表交叉构建与静态链接，不代表 Switch 已经实际呈现。

SD 包包含办公室和标题所需的 `bk3_03.pp`、`bk3_00.pp` 及已有配套 TBL，不打包 EXE、补丁或存档。Switch 默认办公室；左/右摇杆移动/转向、十字键上下升降、A 重置、B 到标题、标题 A 回办公室、`+` 退出。完整内存应用模式要求不变。原素材保持只读。

```sh
./build-host.sh --vulkan
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
build/scene-probe local/game/MAINDIR local/scene-check
build/biko3-preview local/game/MAINDIR local/static-office.rgba --scene office --frames 16
local/venv/bin/python tests/original_static_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
cmake -S . -B build/asan-static -DCMAKE_BUILD_TYPE=Debug -DBK_SANITIZE=ON -DBK_WITH_VULKAN=ON
cmake --build build/asan-static --parallel 8
ctest --test-dir build/asan-static --output-on-failure
build/asan-static/scene-probe local/game/MAINDIR
./build-switch.sh
python3 tools/package_sd.py local/game/MAINDIR
```

当前产物和回读摘要见 [static-scene-verification.json](static-scene-verification.json)。此前模型/材质报告保持为历史快照。下一步是 M1.4：原版光照、雾、相机和同视角视觉对照；设备可用时补 Switch NVK 日志及操作验证。未进入 M2 动画或 M3 玩法。
