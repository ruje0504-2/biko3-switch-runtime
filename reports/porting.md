# 尾行3 Switch / NVK 移植记录

日期：2026-09-27。**目标是完整原生移植；当前仅完成资源层和标题预览，目标尚未达成。**

本记录保留 0.1.0 资源验证与逆向证据。0.2.0 的模块重构、主循环、覆盖规则与分阶段安排另见 [工程架构](../docs/architecture.md) 和 [本轮验证](architecture-verification.json)；旧版 `verification.json` 的二进制哈希只代表当时的基线。

当前输入是 Windows 安装包、汉化/去码补丁与存档，没有游戏或引擎源码。安装包中的 EXE 不能直接在 ARM64/Horizon 上运行。本工程采取原生重建资源读取、渲染与后续游戏逻辑的路线，没有实现 Windows/D3D 兼容层。

## 已落地的部分

| 部分 | 当前结果 | 验证边界 |
| --- | --- | --- |
| 离线提取 | Wise EXE + W02 提取 22 个 PP 及 15 个 TBL | 不执行 Windows 安装程序或游戏 |
| 资源层 | C/Python 两套索引读取器，支持旧式 PP 与 TBL 索引 | 7,081 个条目逐条一致 |
| 图片 | BMP 8/24/32 位、TGA 24/32 位、TGA RLE 与方向处理 | 4,162 张图片与 Pillow 逐像素相同 |
| TBL 解码 | DWORD XOR + LZSS，首字节桶索引 | 15 个文件与原 EXE 的隔离函数输出一致 |
| Vulkan | 纹理上传、显式图形管线、深度、混合、同步、呈现 | 主机 MoltenVK 实际 GPU 绘制与回读；Switch 未实测 |
| Switch | 指定 Mesa NVK、NWindow、loaderless 静态链接、NRO | 编译链接通过，无未解析符号 |
| 程序内容 | 原版标题图片，永久开发提示 | 没有可用游戏菜单或玩法 |
| 模型研究 | 247 个 OBJM 的 chunk/网格/索引边界审计 | 仅分析工具，运行时尚不支持模型 |

所有测试摘要见 [verification.json](verification.json)。完整本地输出为 `local/test-host.log`、`local/original-tbl-oracle.log`、`local/model-audit.json`。生成的本地截图来自 Apple M4 / MoltenVK，不能作为 Switch 实机截图或性能证据。

## 固定依赖与构建问题

- [Mesa Switch 提交](https://github.com/danfromtico/mesa-switch/commit/5dba7886c56460ff47c3038e323d07b9547d6212)：`5dba7886c56460ff47c3038e323d07b9547d6212`，26.2.2；查询时为 main 最新版本。
- [官方构建记录](https://github.com/danfromtico/mesa-switch/actions/runs/35860399051)：run `35860399051`，artifact `10750582639`。
- SDK SHA-256：`230b777e49e6a7efa43b13897d7abc5eec54bea68ff1732e9ff1ef8c63b1adeb`，与 GitHub artifact digest 核对。
- SDK 的 NAK/NIL 使用 `rustc 1.100.0-nightly (6bb1652a0 2026-09-22)`。官方 SDK 静态库未包含配套 Rust 标准库，首次链接因此失败。最终使用完全匹配的 `nightly-2026-09-23`、`aarch64-unknown-linux-gnu` 标准库，并由该 Mesa 分支的 Horizon Rust 适配代码处理平台接口。没有使用空实现掩盖缺失符号。
- Rust 发行包 SHA-256：`d0bce065ca66bdd554f68f402869085e4c66ae1725e0e57bcd51afbb740dc45d`，已与官方 `.sha256` 核对。
- `tools/fetch_sdk.py` 固定 URL 和摘要；依赖置于 `local/`。不修改全局 `/opt/devkitpro` 的 Mesa 安装。
- NRO 直接链接 `libvulkan.a`，没有 Vulkan loader/ICD、OpenGL、Zink 或 deko3d 回退。Switch 创建后核对 `VK_DRIVER_ID_MESA_NVK`，失败时记录真实错误。

参考 [kisaku-switch-runtime](https://github.com/ruje0504-2/kisaku-switch-runtime/tree/e77e7ef9dca666d2351be2afe74e40780c5c6a90) 的平台构建和验证组织。《鬼作》的 AI6WIN VM 不适用于本游戏，未复制其运行时代码，也未继承其任何可玩性结论。

## 逆向依据

下列地址全部是这个特定中文 EXE 的虚拟地址，不能用于其他版本：

- 文件：`尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe`
- 大小：1,736,704 字节。
- SHA-256：`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。
- PE image base：`0x400000`。

原版提取的 `biko3.EXE` 有壳，SHA-256 为 `ad353d250ebb04cc38fc78894111427a20780162e371914b00234c44a7b72606`。主要静态分析使用未加壳的中文 EXE。数据验证仍使用原版安装包提取资源。

| 地址 | 观察到的职责 |
| --- | --- |
| `0x465190` | TBL 加载 |
| `0x4653d2` | 根据文件名首字节查桶，遍历 80 字节记录 |
| `0x497f4a` | 读取解码分配大小 |
| `0x497f63` | TBL LZSS 解码入口 |
| `0x498711` | 完整 DWORD 的 XOR |
| `0x534340` | 解码函数调用的 memset |
| `0x417df2` | 模型网格格式分支 |
| `0x41853a` | 模型网格头部处理 |
| `0x418565` | 332 字节子网格头部 |
| `0x4185a1` | 60 字节顶点跨度 |
| `0x4188ad` / `0x4188b3` | 16 位索引区长度与位置推进 |
| `0x41893e` | 分组网格中的额外子头部 |

`tests/original_tbl_oracle.py` 使用 Unicorn 将 PE 映射到隔离内存，只调用解码入口；唯一替代函数是受地址/大小约束的内存清零。不启动 EXE，不加载 Windows API，不执行游戏流程。它对比有效表区，不对分配缓冲区尾部未定义内容作相等断言。

## 已验证的资源格式

### 内嵌索引的旧式 PP

小端 `uint32 count`、`uint32 payload_total`，随后 `count * 32` 字节文件名、`count` 个 `uint32` 长度，最后串联文件数据。文件名和数据的每个字节按 `(-byte) & 255` 还原。首个数据偏移为 `8 + 36 * count`。索引必须与文件实际大小相符。

### TBL + PP

带同名 `.tbl` 的 `.pp` 是原始数据拼接，不再做逐字节变换。

TBL 首个 DWORD XOR `0xa67f54cb` 得到分配大小。剩余输入的完整 DWORD XOR `0x35d8ca2f`，不足 DWORD 的尾部保持不变。LZSS 使用 4096 字节初始全零环形缓冲，写指针 `0xfee`；标志位从低位消费，1 为 literal，0 为匹配。匹配位置由低字节与第二字节高半字节组成，长度为低半字节加 3，允许重叠复制。

有效解码表长度是分配大小减 256。最后 256 字节属于写入器的工作区，不能当成索引；部分文件不会产生完整的分配长度。有效表的前 2048 字节是 256 个 `(offset, count)` 桶，按文件名第一个字节索引，offset 相对记录区。记录为 80 字节：

| 偏移 | 字段 |
| --- | --- |
| `0` | 记录跨度，80 |
| `4` | 对应 PP 中的数据偏移 |
| `8` | 文件大小 |
| `12` | 本次样本均为 0 的标志字段 |
| `16` | 64 字节 CP932 文件名 |

实现拒绝越界、重复文件名、路径穿越、过大分配和不完整压缩数据。C 层保留 CP932 文件名字节；Python 层解码名称用于分析。当前预览只查找 ASCII 名称 `op_00.bmp`。

### OBJM 模型：结构已审计，运行时未实现

250 个 `.x` 中，247 个是 `OBJM`，3 个为 `xof 0302txt 0032` 标准 DirectX 文本：`bk3_04.pp` 中的 `cam00_03.x`、`cam00_60.x`、`cam01_50.x`。文本模型尚未解析。

OBJM 为 12 字节头部，随后重复 `(fourcc, uint32 size, payload)`。MESH 中的网格头为 72 字节：前 64 字节名称，`+64` ID，`+68` 子网格数。当子网格数不等于 1 时，每个子网格前另有一个 72 字节命名头，其子网格数为 1。

子网格头为 332 字节，`+64` 是顶点数，`+68` 是索引数。每个顶点 60 字节，其中位置位于前 3 个 float，第四个 float 为序号，后续为法线与 UV 等字段。索引是 16 位三角形列表。工具对顶点/索引区边界、有限浮点值和索引范围作全量检查。

统计：5,248 个网格、12,344 个子网格、3,110,274 个顶点、3,308,998 个三角形。观察到 `MATE`、`TEXT`、`MESH`、`FRAM`、`ANIM`、`FOG `、`ENVL`、`MORP`、`LIGH`、`MATA`、`TEXA`、`PART`。`FRAM` 疑似 396 字节记录、`+68` 矩阵、`+0xac` 父 ID、`+0xb0` 网格 ID；这些语义仍需逐字段核对，不能据此宣称层级或动画已恢复。

## 当前程序与数据包

原来的单文件预览已拆为 `runtime/app/`、`runtime/scene/title.c` 和平台/资源/渲染模块。标题场景从原版 `bk3_00.pp/op_00.bmp` 读取 1280×960 图片，在 1280×720 画面居中保持 4:3 比例绘制。底部永久显示 `TITLE RESOURCES ONLY - GAMEPLAY NOT IMPLEMENTED`。程序没有模拟点击成功、关卡进入或存档成功。

Vulkan 使用纹理 staging、显式 barrier、每张呈现图独立的 finished semaphore、单帧 fence 和真实 GPU 回读测试。当前只有固定大小预览；不支持交换链重建、切换分辨率或复杂场景的透明排序。Switch 需要完整内存的应用模式；日志位置 `sdmc:/switch/biko3/nvk.log`。

`交付/SD卡根目录/switch/biko3/` 包含预览 NRO、完整原版 `bk3_00.pp`、配套 TBL、说明和 SHA-256 清单。当前大小和摘要以包内 `manifest.json` 为准。原始输入保持只读；游戏数据与本地交付包被 Git 排除。

汉化补丁里的 `BK3_00.PP` 只有 80 项，原版对应归档有 575 项，不能简单整包替换。0.2.0 已提供通用资源覆盖层和基础归档回退；汉化包的实际挂载、文本字体、去码补丁和旧存档仍未接入。

## 完整移植尚缺的工作

1. 恢复 MATE/TEXT/FRAM 关联、坐标约定、静态场景和原版相机；用逐顶点、逐材质、截图对照验证。
2. 实现 ANIM/ENVL/MORP、骨骼与变形、角色装配；不能仅加载网格就判定角色系统完成。
3. 重建场景状态、路线、追踪 AI、碰撞、任务触发、对话和手柄输入；需要以原版行为建立可重复对照。
4. 接入音频/视频、存档读写、中文资源覆盖和对应文本编码。
5. 在真实 Switch 上验证 NVK 初始化、呈现与输入，然后验证内存占用、稳定性和帧率。
6. 最后按真实游戏流程验证新游戏、各场景、存取档和退出，才能称为可玩移植。

本次没有 Switch 实机连接、实机日志或完整可玩流程记录，因此这些项目全部保持未完成状态。
