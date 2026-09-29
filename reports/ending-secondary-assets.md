# 第二类结局 4D00FA：独立 CPU 资源与镜头初始化

此报告保留CPU装配阶段的历史验证范围。后续47A5D0/47D3CB、UI/音频、实际绘制和阶段重载已经接入；当前状态与新构建见 `ending-stage-lifecycle.md`。下文“未接入生产入口”和旧NRO摘要不代表当前源码。

日期：2026-09-29。机器可读结果：`ending-secondary-assets-verification.json`。

本阶段恢复第二类结局的资源、姿态和镜头初始化，不是完整可玩入口。生产应用仍显式拒绝尚未接入的分支；没有将普通 4CF318 的另一动作选项冒充 4D00FA。完整移植目标未完成。

## 原版依据和实现

原指令固定于 `尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe`，SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。实际资源来自 `local/game/MAINDIR/Data` 日文版。

`game/ending_secondary` 保存五组配置和原 4D0ADD..4D0B97 的目标运算。第二类加载器从 `bk3_09` 读取 `h01_02..h05_02.xan`，对应 `fambom` 的 `h01_02..h05_02.fam`。镜头选项固定为 1，外层背景选项独立。主角根平移、朝向和相机朝向分别来自 570D6C、570C9C、570C24；571008 的 20 个相机表项按原位模式保存。

`scene/ending_secondary_assets` 独立拥有主体、面部、眼资源、两条真实相机轨道和可选背景。森林编号为主体 0、轨道 1/2、背景 3；没有辅助演员或 BOM。资源解码后依原顺序执行全局挂接、初始朝向、面部 warm-up、根放置、三个节点显隐、主体 configured request 1、flow16 相机轨道加载、39 节点查询、目标快照和固定相机。第二组加载 `m02_90.xan`，其余组由外层背景阶段继续装配；重复加载不重复注册。

目标输入是已经发布的节点 0 与 `A_okosi` 世界坐标。第三向量严格为 `X=anchor.X`、`Y=(node.Y-anchor.Y)/2`、`Z=(node.Z-anchor.Z)/2`，不是两点中点。中间减法不提前舍入成 float。后续跟随目标为 `A_kuch`；仅第三组查询 `OYU`，资源缺失不能用于清除其他组的进程保留字段。

该加载器保留原 4DF411 设置的 FOV 0.2，没有复制普通加载器后续的 FOV 1。初始眼纹理由原 4D080E/4D081E → 4DFB96 → 4A07A9 路径选择槽 1；此前报告将它误记为没有眼槽选择，现已纠正，并补充真实原眼资源加载与选择的对照断言。普通与 ASan 的十入口检查均通过；最新内存检查记录为 `local/ending-secondary-assets-eye-asan.json`。这里的 FOV 是加载时状态，不主张最终游戏帧继续使用同一 FOV。

## 验证

| 检查 | 主机及 ASan/UBSan 结果 |
| --- | --- |
| 固定 EXE 配置和目标 | 各 5 组配置、4,096 次无 hook 原目标运算、4,096 次输入输出重叠；逐字节一致 |
| 非有限目标 | 各 18 次原子拒绝；这是明确的可移植策略，不宣称原非有限结果等价 |
| 真实加载器装配 | 各 10 个入口，32,895 份 local/world/parent 矩阵、72 份片段状态、390 次节点映射、10 次独立原面部初始化；最大归一化误差 0 |
| 资源生命周期 | 各 10 次装配、4 个缺失资源前缀、8 次背景失败后重试；失败不改调用方镜头、预设和 RNG，目标快照不被后续挂接改写 |
| 架构 | 2 项模块依赖和原生 API 隔离检查通过 |
| Switch | 锁定 Mesa NVK 交叉构建通过，独立 `nm -u` 输出为空 |

原装配回放直接执行 4D00FA 和后续 4D460B；文件/模型解码、Windows 堆和 UI/媒体/设备是明确的服务边界。数学、树挂接、显隐、选片、目标捕获及相机函数没有替换。面部 warm-up/RNG 在另一份原控制器 VM 独立比较。该范围不覆盖原 UI、视频或完整第二阶段父循环。LeakSanitizer 关闭，不声称 LSan 通过。

验证入口：

```sh
PYTHONPATH=tests:tools local/venv/bin/python tests/original_ending_secondary_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe'
PYTHONPATH=tests:tools local/venv/bin/python tests/original_ending_secondary_assets_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
build/ending-secondary-probe local/game/MAINDIR/Data
ASAN_OPTIONS=detect_leaks=0 build/asan/ending-secondary-probe local/game/MAINDIR/Data
```

ASan 的 Python 对照使用已预链接 sanitizer 的 `build/asan/ending-oracle-python`，并指定 `BK3_BUILD_DIR=build/asan`。不能改回直接向普通 Python 晚加载 ASan 的旧失败方式。生命周期探针已加入 `test-host.sh`。

当前 NRO 为 `build-switch/biko3-preview.nro`，15,839,288 字节，SHA-256 `042e93e3037a170732f388cbf7d25cc99298b0d690a108daa3a3f89b5d3b715b`。新 CPU 组件已交叉编译，但未被生产场景引用，因此 NRO 哈希不变；不能据此声称第二类结局已在 Switch 可玩。

## 后续真实流程

继续恢复 4D00FA 的进程保留字段和尾部状态、UI/视频/纹理替换生命周期、47A5D0 父控制和 47D3CB 表现，再按本阶段真实拓扑接入场景和绘制。随后仍有其他独立加载器、自然结局和解锁写回，以及完整 Windows/实机画面对照。普通结局的三个未通过像素配对已单独缩小范围，见 `ending-raster-origin.md`，尚未修复。保持 4:3、防休眠及冻结交付。
