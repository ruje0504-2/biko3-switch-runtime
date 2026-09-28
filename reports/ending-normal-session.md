# 普通结算场景应用接入

2026-09-28。新增 `scene=ending` 应用入口，按统一 `load -> step -> draw -> after_present` 生命周期装配普通结算。入口挂载 `bk3_02/bk3_03/bk3_04/bk3_08..bk3_13/bk3_18/fambom`，使用原 0x18 gallery 分支的两个已对照 loader：4cf318 对应 variant 0，4d00fa 对应 variant 1。CPU owner、forest/background、GPU normal renderer、材质缓存、AVI 和 48 个连续音频槽由同一场景拥有；未归属的 loader 继续显式失败。

两个 variant 各运行 64 帧，实际 Vulkan 统计分别为 8,060/4,030 draws、520 次 GPU skin dispatch 和 62,400 音频帧；普通 Vulkan 与 ASan Vulkan 结果一致，应用入口普通版和 ASan Vulkan 各连续呈现 8 帧。首帧在构造阶段准备，之后允许没有固定步进 tick 的刷新帧重复绘制上一快照，解决高刷新率下的空快照错误。

主机回归：普通 CTest 100/100、ASan CTest 91/91、Python unittest 29 项通过；`test-host.sh` 已加入两个普通结算 variant 的资源探针。指定 Mesa NVK 交叉构建通过，Mesa commit `5dba7886c56460ff47c3038e323d07b9547d6212`，NRO SHA-256 `6139f31076f21a53553b3a25aa03caf837e4d816dfd41590147822e9e750acf3`，未解析符号 0。

本页完成的是普通结算场景的应用级接入，不等于完整 flow16。存档复位、特殊双镜头、完整结算 UI/音频调度、解锁生产和 Switch 结算实机仍由后续阶段接入；雨天实机已由用户确认正常，天气交付范围保持冻结，Mac 防休眠继续。
