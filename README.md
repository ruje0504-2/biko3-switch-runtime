# 尾行3 / Biko3 — Switch 原生移植工程

**当前优先移植日文版，默认已接入原标题→五人选择→原版开场剧情→游戏，完整移植仍未完成。** 保留玩家与 NPC 更新、跟随镜头、碰撞、动态角色、HUD、拍照、暂停/恢复、返回标题和确认退出。公共黑幕、暂停菜单和加载页共享持久状态；暂停期间仅更新持续背景声音的音量，动画、计时与随机数保持。新入口已通过主机真实素材、Vulkan 与内存检查回放，尚未获得这版入口的 Switch 实机反馈；此前存档和撞车修正已获用户实机确认。

失败与重试、区域完成保存询问、50 槽保存/覆盖/读取，以及保存后继续下一区域已接入。暂停菜单第一项进入读取页；取消返回原暂停状态，确认读取会重建所选角色的区域入口。存档写入独立输出目录的 `save/checkpoint-0..4.bks`，含版本、组别和 CRC；损坏文件明确失败。标题已显示原版选项，设置、鉴赏及结局解锁持久化仍未接入，到达未绑定流程会明确报错。实际游戏已接入原版雨幕和雪花，随区域加载、暂停和失败流程更新；天气设置菜单、其他任务结算和完整媒体仍在开发，见[天气修复](reports/weather-integration.md)。结果见 [原标题与选人接入](reports/original-front-end.md)、[存读档流程](reports/save-session.md)。主机使用 MoltenVK，不能替代 Switch NVK 实机验证。

构建版本字段仍为 **0.3.5**，原 `交付/SD卡根目录` 为该版本的历史诊断包；新源码的开发包应输出到独立目录。阶段历史保留于 `reports/`，较早报告中的“尚未接应用”等描述仅指该报告当时的范围。整体设计见 [架构与模块边界](docs/architecture.md)，实施顺序见 [分阶段路线](docs/roadmap.md)。

《尾行3》是 Direct3D 时代的 3D 游戏，与《鬼作》的 AI6WIN 不同。本项目参考 [kisaku-switch-runtime](https://github.com/ruje0504-2/kisaku-switch-runtime) 的本地素材隔离、主机验证和 Switch 打包方式；没有把它的 VM 或可玩性结论套用到本游戏。

渲染器直接静态链接 [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) 的 NVK，使用 `VK_NN_vi_surface` / `NWindow`。没有 OpenGL、Zink、deko3d 回退。2026-09-27 核对主分支最新提交为 `5dba7886c56460ff47c3038e323d07b9547d6212`，Mesa 26.2.2。启动会把设备名称及 driver ID 写入日志，并拒绝非 NVK 的 Switch 驱动。

## 验证记录

- 22 个归档、7,081 个条目：C 与 Python 索引逐条一致。
- 4,162 张 BMP/TGA：原生解码结果与 Pillow 逐像素一致。
- 15 个 TBL：与原 EXE 的隔离解码函数对照，全部表数据一致。
- 合成图、原版标题、透明按钮：各连续 16 帧真实 Vulkan 绘制，回读误差不超过 1/255。主机使用 MoltenVK，不能当作 Switch NVK 实机结果。
- 12,000 组畸形图片/TBL：AddressSanitizer / UBSan 通过；资源测试和架构边界检查见验证记录。
- Switch NRO 生成，最终 ELF 无未解析符号。
- 0.2.0 架构重构：8 项 Python 测试、4 项主机 CTest、3 项 ASan/UBSan CTest 通过；标题连续 16 帧后回读与 0.1.0 基线逐像素一致。
- 0.2.1 模型/材质基础：17 项 Python、6 项主机 CTest 通过；CPU 模型变异检查和原版材质加载通过 ASan/UBSan。新增索引网格、混合、深度、采样与状态恢复的 GPU 回读检查；标题回归不变。光照/场景和实机验证仍未完成。
- 0.3.0 静态场景：20 项 Python、8 项主机 CTest、8 项 ASan/UBSan CTest 通过；原版办公室也通过 sanitizer 下的加载/绘制/切换检查。原 EXE 隔离队列分支 1,324 次、排序 102 组对照通过。连续 16 帧一致；相机移动改变画面、重置后逐字节恢复；三轮办公室/标题切换通过，标题与旧基线完全一致。测试范围与限制见 [机器可读记录](reports/static-scene-verification.json)。
- 0.3.1 点光照：23 项 Python、9 项主机 CTest、9 项 ASan/UBSan CTest 通过；新增 71 次原版光源提交/变换、128 次环境光量化对照，10 组独立公式 GPU 回读及光源描述符隔离检查。办公室连续帧、移动/重置、三轮场景切换也通过 sanitizer；标题仍与旧基线逐字节相同。GPU 公式检查不代表 Windows 原版画面对照，详见 [该阶段验证记录](reports/lighting-verification.json)。

- 0.3.2 相机基础：259 组投影、277 组视图矩阵（含 21 个原资源相机节点）、45 组初始化选择及 128 组相机动画推进量对照通过。主机和 ASan/UBSan 各 11 项 CTest 通过；修正最终裁剪参数后，受影响的相机/几何检查及原办公室生命周期检查再次通过。新增三种画幅的逐像素视口/裁剪、UI 和下一帧恢复检查；标题仍保持旧基线。范围见 [该阶段验证记录](reports/camera-verification.json)。

## 构建

需要 CMake 3.24+、现有 devkitPro/devkitA64、libnx、Switch expat/zstd/zlib、Python 3.12+、glslangValidator。版本和下载摘要统一在 `config/dependencies.lock.json`；依赖放在本项目 `local/`，不会替换系统的旧 Mesa。

```sh
python3 tools/fetch_sdk.py
./build-switch.sh
```

官方 CI SDK 的静态库没有带齐 Rust 标准库。构建使用与 SDK 完全一致的 `nightly-2026-09-23` AArch64 标准库；下载包校验 SHA-256。GitHub Actions 附件有保留期限，建议保留已验证的 `local/mesa-sdk.zip` 与 `local/rust-std.tar.xz`。附件过期时脚本明确失败，不会静默升级依赖。

主机资源检查：`./build-host.sh`。macOS Vulkan 检查还需 MoltenVK：`./build-host.sh --vulkan`。`MOLTENVK_PREFIX` 可指定其安装前缀。

主机使用离屏验证入口，可指定帧数和场景：`build/biko3-preview local/game/MAINDIR build/office.rgba --scene office --frames 16`。省略 `--scene` 默认 `game`，首屏为原版标题；显式 `--scene title` 保留旧标题素材检查。`BK_DEVELOPMENT_ENTRY=1` 仅供旧开发流程回归。主机入口不是桌面可玩窗口。生成目录为 `build/`、`build-switch/`；无需素材的合成测试可直接运行 `./test-host.sh`。

## 原版素材

原始 `尾行3 [汉化]/` 保持只读。安装包及 W02 同目录，离线提取命令：

```sh
python3 tools/prepare_game.py '尾行3 [汉化]/安装/biko3.EXE'
python3 tools/bk3_assets.py local/game/MAINDIR/Data --json local/resource-index.json
```

提取器输出已存在时不覆盖，可传 `--output` 使用新目录。本次已经提取完成，不必重跑。提取及资源读取不启动 Windows 游戏。当前使用日文原始资源与 Type_S/G 字库，不挂载汉化或去码补丁；原版存档不会自动导入或覆盖。流程分析仍以固定摘要的可分析 EXE 为依据，日文安装 EXE 的保护壳尚未解出，证据限制见存读档报告。

## SD 卡预览

```sh
python3 tools/package_sd.py local/game/MAINDIR
```

以**完整内存的应用模式**打开新构建的 `switch/biko3/biko3-preview.nro`。默认进入原版标题，日志为 `sdmc:/switch/biko3/nvk.log`；照片和临时暂停截图写入 `sdmc:/switch/biko3` 下的独立输出目录。

| 操作 | 当前作用 |
| --- | --- |
| 标题十字键 / A | 选择并确认；新游戏进入原版五人选择 |
| 左摇杆 / 十字键 | 游戏移动；菜单中十字键选择 |
| 右摇杆 | 游戏视角 |
| 结局 L + 右摇杆 | 按摇杆方向旋转镜头并显示对应提示图标，手动速度为最初的 6 倍 |
| 结局 R + 右摇杆 | 右推拉近、左推拉远，上推升高、下推降低，并显示对应提示图标；手动速度为最初的 6 倍 |
| A | 对白推进 / 交互 / 菜单确认 |
| B | 游戏姿态切换 |
| X / Y | 游戏镜头切换 / 拍照 |
| ZL | 慢走 |
| `+` | 游戏允许操作时打开暂停菜单 |

默认打包当前游戏和诊断入口所需的 12 个 PP（含选人资源 `bk3_18.pp` 和天气资源 `bk3_20.pp`）、配套 TBL，以及 CKP/ATR/FAM/FTT 散文件；`--full-data` 可复制全部 22 个归档。所有复制文件校验 SHA-256，不打包 EXE、原存档或补丁。素材仅用于本地自有副本，不属于源码许可范围。历史 SD 包不会自动更新；新包须用 `--output` 指定独立目录。

需要把原版 `Data/` 下的完整游戏数据交付时，使用 `--complete-data`；它会复制全部 168 个数据文件（包括 PP、TBL、关卡、角色、字体、配置和媒体数据），仍不打包 EXE、原存档或补丁：

```sh
python3 tools/package_sd.py local/game/MAINDIR \
  --complete-data --output '交付/完整游戏数据-20260928'
```

## 开发验证

```sh
python3 -m venv local/venv
local/venv/bin/pip install -r tests/requirements.txt
./test-host.sh local/game/MAINDIR
local/venv/bin/python tests/original_tbl_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
python3 tools/inspect_objm.py local/game/MAINDIR/Data
python3 tests/audit_models.py local/game/MAINDIR/Data
build/model-probe local/game/MAINDIR/Data/bk3_03.pp m01_04.x
build/material-probe local/game/MAINDIR/Data/bk3_03.pp m01_04.x
build/material-render-probe local/game/MAINDIR/Data/bk3_03.pp m01_04.x
build/scene-probe local/game/MAINDIR build/scene-readback
local/venv/bin/python tests/original_static_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
build/lighting-render-probe
local/venv/bin/python tests/original_lighting_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
python3 tools/inspect_cameras.py local/game/MAINDIR/Data/bk3_04.pp
build/camera-render-probe
local/venv/bin/python tests/original_camera_oracle.py \
  '尾行3 [汉化]/去码汉化补丁/汉化/尾行3中文版.exe' local/game/MAINDIR/Data
```

后续按 [分阶段路线](docs/roadmap.md) 继续完整任务生命周期、音视频、存档及中文。原版证据见 [移植记录](reports/porting.md)，M0 快照见 [架构验证](reports/architecture-verification.json)。通用资源覆盖规则已接通；应用尚未挂载汉化包。
