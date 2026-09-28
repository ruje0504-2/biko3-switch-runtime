# M1.4 点光照子步骤 — 0.3.1

办公室 `bk3_03.pp/m01_04.x` 已接入原资源的一个环境光、六个点光源，以及材质环境色、漫反射、自发光。保留 FRAM 基础姿态、检查相机、原版纹理、静态队列和标题切换。**M1 整体验收尚未完成：没有原版同视角画面对照、原版相机/时间绑定或 Switch 实机证据，也不能进入玩法。**

本记录取代当前状态中的 0.3.0 无光照说明；`static-scene.md` 及对应 JSON 保留为历史快照。机器可读结果见 `lighting-verification.json`。

## 原版数据与隔离代码证据

分析对象为中文 EXE，SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`，PE 基址 `0x400000`。以下地址仅适用于该文件。对照通过 Unicorn 执行隔离函数并捕获提交参数，不启动 Windows 游戏，不执行原版 Direct3D 光栅化。

办公室 LIGH 为 1,204 字节，包含七条 172 字节记录：

| 记录偏移 | 内容 |
| --- | --- |
| 0 / 64 / 68 | 64 字节名称、ID、灯光类型 |
| 72 / 88 / 104 | diffuse / specular / ambient，各 RGBA float |
| 120 / 132 | position / direction，各三个 float |
| 144 / 148 | range / falloff |
| 152 / 156 / 160 | 常数、线性、二次衰减 |
| 164 / 168 | theta / phi |

类型 `0xffffffff` 是原游戏自定义环境光，类型 1 为点光源；其余方向/聚光类型当前明确拒绝。`0x4174b0` 的加载循环按 `0xac` 步进，`0x425c50` 将从记录 `+0x44` 开始的 104 字节复制到灯光对象。

`0x425f79` 对环境光取 diffuse RGB，乘 255 后截断为整数并提交渲染状态 139。办公室 `(0.2f, 0.15f, 0.1f)` 得到 **ARGB `0xff332619`**，不是四舍五入的颜色。C 适配器保留同一量化；其他灯光向设备提交 104 字节灯光参数。

FRAM 每条 396 字节，`+180` 为灯光 ID。`0x421dd4 → 0x4260b6` 将点光位置**替换为关联节点的世界平移**，不能再次变换 LIGH 中已导出的坐标。本步把这一规则独立放入 CPU 环境适配器。办公室七盏灯关联节点 57–63，父灯光节点为 56。

| 点光 | diffuse RGB | 世界位置（约值） | range | 衰减 `(a0,a1,a2)` |
| --- | --- | --- | --- | --- |
| 1 | 1, .6, .4 | 0, 31.85, 0 | 80 | 0, .03, 0 |
| 2 | 1, .3, .4 | 37.74881, 24.75, 54.10088 | 150 | 0, .1, 0 |
| 3 | .2, .4, .6 | -53.214447, 25.899998, -56.983467 | 30 | 0, .05, 0 |
| 4 | .6, .8, 1 | -.496869, 26.699998, 103.117195 | 60 | 0, .03, 0 |
| 5 | 1, 1, 1 | -105.03649, 30.699978, -1.8587168 | 20 | 0, .05, 0 |
| 6 | 1, 1, 1 | -48.720936, 29.149996, 47.39107 | 100 | 0, .1, 0 |

办公室 64 个材质的 power 和 specular 全为零。材质 ambient 全白；自发光包括零、白色和 `(0.3,0.3,0.3)`。当前环境适配器明确拒绝需要高光路径的材质，不能以零高光替代未实现行为。

FOG 为 32 字节：enabled/mode/table/range-based/color 五个 u32，随后 start/end/density 三个 float。办公室值为 `(0,3,0,0,0xffffffff,0,10000,0)`。原加载片段 `0x41ae69..0x41af06` 在 enabled=0 时跳过雾状态提交；它不保证清除之前场景继承的雾。当前独立办公室预览从关闭的雾开始，启用的雾明确报不支持。

原版 `0x4a4701` 存在环境光选择及按绘制阶段选灯的分支，其中 `0x4a48d0` 路径开启全部非环境灯，其他路径有选择性开关。当前静态预览使用这六个点光，不声称恢复完整游戏的各遍选灯策略。`0x4296c4` 设置法线归一化，`0x429681` 设置局部观察者；当前办公室无高光，不使用观察者位置计算高光。

## 渲染实现与架构

- `core/lighting` 定义不依赖 GPU 的环境光/至多八个点光数据，检查有限值、非负颜色、范围和有效衰减。
- `model/environment` 解析并拥有 LIGH/FOG 副本，检查 ID、节点关联和支持范围，产生通用灯光描述；不创建 GPU 对象。
- `scene/static_world` 负责上传带原始法线、材质环境色和自发光的网格，持有不可变灯光集合，逐实例提交 MVP 和世界矩阵。
- `render` 使用单独光照顶点格式和管线；uniform 集合与 descriptor 不可变，每次绘制显式绑定，创建/销毁等待在途 fence。标题和普通网格保留原路径。

法线使用世界矩阵逆转置并归一化；光照在世界空间逐顶点计算。每个点光在范围内使用 `attenuation = 1/(a0+a1*d+a2*d*d)`，点光方向与法线点积取非负值。环境项乘材质环境色，漫反射项乘材质漫反射，自发光独立相加；顶点 RGB 截断至 `[0,1]` 后插值，最后与纹理相乘。alpha 继续使用已有材质与纹理规则。该公式参考微软固定管线文档：[光照数学](https://learn.microsoft.com/en-us/windows/win32/direct3d9/mathematics-of-lighting)、[环境光](https://learn.microsoft.com/en-us/windows/win32/direct3d9/ambient-lighting)、[漫反射](https://learn.microsoft.com/en-us/windows/win32/direct3d9/diffuse-lighting)、[衰减](https://learn.microsoft.com/en-us/windows/win32/direct3d9/attenuation-and-spotlight-factor)。这些 D3D9 文档用于数学说明，原游戏的实际参数和状态以本节原代码为证据，不声称原游戏使用 D3D9。

距离为零时漫反射贡献置零，非正分母跳过，避免 NaN；这些退化值没有原光栅器对照。奇异世界矩阵、错误网格格式、跨 renderer 对象明确拒绝。当前未实现高光、方向光、聚光、启用的雾、灯光动画或游戏选择策略。

## 已执行验证

| 检查 | 结果与范围 |
| --- | --- |
| 原版隔离函数 | 七条办公室记录，包括六条完整 104 字节点光提交；另 64 组非单位节点变换，共 71 次通过 |
| 环境光量化 | 128 组随机 float32 RGB 与原版 x87 截断结果一致 |
| 禁用雾分支 | 原版办公室 FOG 加载片段未提交雾状态 |
| Python | 23/23；新增布局/关联/量化、畸形值和所有 344 种灯光截断长度检查 |
| 主机 CTest | 9/9，包含实际 Vulkan 光照回读 |
| ASan/UBSan CTest | 9/9；另运行原版办公室加载、绘制、切换检查，通过 |
| 光照 GPU | 十组独立双精度公式/插值参考，RGBA 误差不超过 2/255；覆盖背光、范围、线性/二次衰减、非均匀/反射/平移矩阵、法线归一化、截断、两灯/八灯和纹理 alpha |
| GPU 状态隔离 | 同帧绑定不同灯光集合、光照/普通格式拒绝、奇异矩阵拒绝、UI 恢复，通过；增强检查在普通及 sanitizer 构建均单独重跑通过 |
| 原办公室 | 连续 16 帧一致；移动改变画面，重置逐字节恢复；三轮办公室/标题切换，通过；缺失素材创建失败且已有场景仍可使用 |
| 标题回归 | 与先前标题基线逐字节一致 |
| Switch | 0.3.1 ARM64/NVK NRO 构建通过，ELF 未解析符号 0；未在 Switch 上运行 |

主机设备为 Apple M4，MoltenVK 1.4.2，办公室回读 1280×720。公式对照使用独立 CPU 计算和高斯消元求法线变换，不执行原游戏光栅化。程序入口与 scene-probe 的办公室回读逐字节一致。结果摘要：

| 产物 | SHA-256 |
| --- | --- |
| 办公室 RGBA | `598c6032a9597adcce187e54b8e44ffafc2fe1fdb46dc7b057ca0f82331615c9` |
| 移动后 RGBA | `ad4f9ce0c93796e228f87f247230392262d0b2f80c69ef5c04116d9091ffd88e` |
| 标题 RGBA | `8f6ed15f5f8df8c022d85cb6895f6c5fa7146930508bfc5c4bec7f0afb7e3b7d` |
| Switch NRO | `1295a148c6292d06f920ad8356ce248c7b5bd06ae43ad6f22acc219b31a90b52` |

Mesa 继续固定为 26.2.2 / `5dba7886c56460ff47c3038e323d07b9547d6212`，Switch 启动仍要求 NVK driver ID。交付目录为 `交付/SD卡根目录/switch/biko3`，预览图为 `交付/预览/office-0.3.1.png`；NRO/归档的逐文件打包及回读校验结果存入本阶段 JSON。

复现主要检查（原始素材保持只读）：

```sh
./build-host.sh --vulkan
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
local/venv/bin/python tests/original_lighting_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
build/lighting-render-probe
build/scene-probe local/game/MAINDIR local/lighting-scene
cmake -S . -B build/asan-static -DCMAKE_BUILD_TYPE=Debug \
  -DBK_SANITIZE=ON -DBK_WITH_VULKAN=ON -DBK_BUILD_TESTS=ON
cmake --build build/asan-static --parallel 8
ctest --test-dir build/asan-static --output-on-failure
build/asan-static/scene-probe local/game/MAINDIR local/lighting-asan-scene
./build-switch.sh
python3 tools/package_sd.py local/game/MAINDIR
```

## 下一步与尚未验收部分

`bk3_04.pp` 已找到 15 个 cam*.x：12 个 OBJM，包含 FRAM/ANIM；三个 DirectX 文本模型 `cam00_03.x`、`cam00_60.x`、`cam01_50.x` 尚无解析器。候选节点名包括 Cam_AUTO、Cam_01–03、cam、camloop；导出器的 persp/top/front/side 不能直接视为游戏镜头。完整盘点见 `camera-inventory.json`，工具为 `tools/inspect_cameras.py`。

EXE 中办公室资源路径、相机文件表和 Cam_AUTO 名称提供后续追踪入口，但尚未证实办公室与某一相机文件、节点、播放时刻的对应关系。下一步核对调用关系与时间绑定，再补同视角画面对照；同时保留完整节点遍历/距离计算、各遍选灯策略和实机验证的待核对状态。当前不进入 M2，也不据这张检查视图宣称原版视觉已复现。
