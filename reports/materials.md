# M1.2：材质状态与纹理绑定

本阶段完成 CPU 材质状态、纹理绑定及 Vulkan 混合/索引缓冲接口。**原版场景尚未接入画面**：GPU 验证是合成几何和材质测试色块，不能作为原版场景、照明或 Switch 实机画面对照。M1 整体仍未验收。

## 已实现

- `runtime/model/material.[ch]`：保留编码 alpha，还原提交的 RGBA、混合方式、可见性、原版 alpha 缓存提示和高光开关，不依赖 Vulkan。
- `bk_model_texture_load`：通过 resource store 按明确的 `(pack, TEXT.filename)` 读取 BMP/TGA。调用方拥有 CPU 图片；缺失、损坏或不支持的格式明确失败，不生成默认纹理。
- `BkGpuMesh`：持久顶点/16 位索引缓冲，上传复制 CPU 数据，实例绘制时传入矩阵；调用方明确指定混合与深度写入，支持重复/钳制采样。
- 模型材质使用相同的 RGB/alpha 混合因子；UI 保留旧版的独立 alpha 行为。网格与 UI 混合提交时恢复各自管线和顶点缓冲。

纹理与网格属于创建它们的 renderer，创建/销毁须在活动帧之外并早于 renderer 销毁。CPU 模型和 GPU 缓存独立释放。shader 当前仅将采样纹理乘以调用方提供的顶点色，**没有实现原版固定管线光照、雾或透明排序**。场景到模型的调用尚未接通，标题 NRO 不加载模型。

## 原版证据

所有地址仅适用于中文 EXE SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

| 地址 | 核对结果 |
| --- | --- |
| `0x43041a` | 复制 68 字节材质数值，处理 alpha 范围与阈值 |
| `0x43056d` | 还原实际 alpha、提交混合因子及材质；power ≥ 0.01 打开高光 |
| `0x444c08` | 材质 alpha 等于零时跳过网格 |
| `0x44241c..0x442479` | 按 TGA 后缀设置纹理 alpha 标志，不扫描像素；原版还有 DDS 分支，当前不支持 DDS |
| `0x443364` | 绑定纹理时更新 alpha 缓存提示 |
| `0x4186c8` / `0x4186eb` | 子网格 `+24..+36`、`+40..+52` 是额外纹理绘制的源/目标混合因子数组 |
| `0x445206` / `0x445228` | 从第二个纹理开始使用上述数组并重新绘制；它们不是第一阶段的纹理运算符 |
| `0x4294b5` | 初始化设备 AlphaBlendEnable 为真 |

常见 alpha 编码如下。代码保留原 EXE 的 float32 阈值、严格比较及负零，表格不是边界值的完整定义。

| 编码 alpha | 实际 alpha | RGB 结果 |
| --- | --- | --- |
| `0` | `0` | 跳过网格 |
| `0 < a ≤ 1` | `a` | `源色 × 源alpha + 背景 × (1−源alpha)` |
| `1 < a ≤ 2`，超出近 1 阈值 | `a−1` | `源色 × 源alpha + 背景` |
| `−1 ≤ a < 0`，超出近 0 阈值 | `−a` | `背景 × (1−源色)` |

实际源 alpha 还会乘以纹理 alpha。`0x645608` 是材质路径写入的缓存提示；`0x43056d` 没有根据它发出 AlphaBlendEnable 调用。因此新接口没有把这个提示直接映射为关闭 Vulkan 混合。

原版因子分别为 `(5,6)`、`(5,2)`、`(1,4)`，名称对应关系见 [Microsoft D3DBLEND](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dblend)。实现依据是原版函数对照，文档用于解释枚举名称。

## 验证结果

`tests/original_material_oracle.py` 在 Unicorn 中隔离运行材质设置、应用及网格可见性检查，仅替换 memcpy 和两个 D3D 设备调用并捕获提交数据。不启动 Windows 游戏或系统接口。

- 247 个 OBJM 的 9,710 个材质，分别带/不带纹理 alpha 标志：19,420 次检查；阈值、相邻 float32、负零、范围截断及随机值：2,074 次。合计 **21,494 次通过**。
- 对照有效 diffuse 的全部字节、处理后的编码 alpha、混合因子、可见性、缓存提示及高光开关。
- `bk3_03.pp/m01_04.x` 办公环境：42 张纹理，解码后 6,060,032 字节，其中 3 张有 TGA alpha 标志。66 个子网格为普通混合 56、加法 4、反色 6，其中 4 个 alpha 为零而隐藏。
- 42 张纹理实际上传至主机 Vulkan；66 个材质使用合成四边形取纹理首像素形成测试色块，回读 RGBA 与独立混合公式的误差 ≤ **2/255**。此检查不覆盖整张纹理采样、原版几何或照明。
- 合成 GPU 检查覆盖三种混合、隐藏、索引上传、深度写入、重复/钳制采样、实例矩阵以及网格/UI 状态恢复。
- 主机 CTest 6/6、Python 17/17 通过。材质加载在 ASan/UBSan 下通过原版办公场景验证。
- 更新渲染器后的原标题连续 16 帧回读，与旧标题基线逐字节一致。
- Switch 0.2.1 NRO 已生成，最终 ELF 未解析符号为零，功能仍为标题预览。

主机 GPU 是 Apple M4 / MoltenVK 1.4.2；Switch NVK 尚无实机结果。机器可读证据见 [material-verification.json](material-verification.json)。M1.1 的 [model-verification.json](model-verification.json) 保留为当时的阶段快照，不能将其中旧构建库摘要当作当前产物摘要。

## 复现

```sh
./build-host.sh --vulkan
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
local/venv/bin/python tests/original_material_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
build/material-probe local/game/MAINDIR/Data/bk3_03.pp m01_04.x
build/material-render-probe local/game/MAINDIR/Data/bk3_03.pp m01_04.x
./build-switch.sh
```

## 下一步 M1.3

在 scene 层上传实际子网格并按 FRAM 生成实例，定义唯一的矩阵/视图投影适配，核对绕序、深度与 UV。原版光照、雾、透明排序和相机仍需证据。

当前材质测试入口只接受有材质、至多一张纹理的子网格。真实几何适配还必须处理或明确拒绝子网格 `+56` 的非三角形列表拓扑、`+60` 等状态及额外纹理绘制，不能静默套入现有 triangle-list API。未进入 M2 动画或 M3 玩法。
