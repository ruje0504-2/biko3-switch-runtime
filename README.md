# 尾行3 / Biko3 — Switch 原生移植工程

**当前为日文开发版 0.3.5，主要游戏功能已接入。** 使用 Nintendo Switch 原生输入、音频和 Vulkan NVK 渲染，画面保持 4:3，在 1280×720 输出中居中显示为 960×720。

已接通原标题、五人选择、开场剧情、游戏跟踪、失败与重试、暂停与返回标题；结局、特殊场景、鉴赏回放、音量设置和相册均有实际应用入口。游戏支持动态角色、跟随镜头、碰撞、雨雪、HUD 与拍照，存读档、下一段剧情交接、动作记录、解锁、照片和音量配置也已接入。

这是持续修复中的开发版本，尚未完成全部角色自然路线和最新构建的 Switch 整体验收。已有主机检查按各报告的范围记录，不等同于实机验收；后续优先修复实际反馈的问题。当前继续使用日文资源和日文操作说明，中文补丁另行更新。

## 最近更新

- **减少渲染重复提交**：缓存同一命令缓冲中的图形状态绑定；固定测试片段中，绑定调用从 2,291,089 次降至 1,247,714 次，减少约 **45.5%**，画面和照片保持一致。[验证记录](reports/vulkan-bindings.md)
- **减少 NPC 无效遮挡计算**：视野外或已经确认被遮挡后停止继续检查视线三角形，地面高度与行为仍正常更新。[验证记录](reports/npc-occlusion-performance.md)
- **修复藏身退出卡住**：共用退出逻辑在动画结束时将水平位置归回进入点，修复剑道少女第五关藏身物边角退出后无法移动的问题。[验证记录](reports/hiding-exit.md)
- **修复相册与补充操作**：修复 Switch 相册黑底和照片打不开；剧情中按住 R 强制快进，FPS 默认关闭，可用 − 切换。[验证记录](reports/album-switch-fix.md)

以上调用量变化不是总 CPU 用量或实机 FPS 的降幅。Switch 矩阵与 CPU 蒙皮也已使用 NEON intrinsics 和少量内联汇编预取，并完成 ARM64 原版数值对照；具体范围见 [NEON 验证](reports/neon-skinning.md)。最新 Switch 性能收益尚未测量。

当前本地构建为 `build-switch/biko3-preview.nro`，16,318,520 字节，ELF 未解析符号为 0；SHA256：

```text
da16a25a097169dc0d027a4999c79e42a8c0fe2a8bdba561a96b0c6539ac68fc
```

旧的 `交付/SD卡根目录` 是历史包，不会随源码或 GitHub 推送自动更新。更新已有安装时可单独替换 NRO；只有所需数据文件发生变化时才需要补充素材。

《尾行3》是 Direct3D 时代的 3D 游戏，与《鬼作》的 AI6WIN 不同。本项目参考 [kisaku-switch-runtime](https://github.com/ruje0504-2/kisaku-switch-runtime) 的本地素材隔离、主机验证和 Switch 打包方式。

渲染器直接静态链接 [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) 的 NVK，使用 `VK_NN_vi_surface` / `NWindow`。当前锁定提交 `5dba7886c56460ff47c3038e323d07b9547d6212`，Mesa 26.2.2；启动日志记录设备名称和 driver ID，并校验 Switch 使用 NVK。依赖版本见 [锁定文件](config/dependencies.lock.json)。

## 验证与当前范围

本批代码已完成针对性的普通与 ASan/UBSan 检查、真实素材输入回放和指定 Mesa NVK 交叉构建。渲染优化前后及 ASan 版本各回放 1,054 个游戏帧，最终画面和两张照片逐字节一致；NPC 查询与原版的 1,392 组结果一致；藏身退出修复通过同点边角复现及实际应用回放。详细数据分别见上方报告。

此前雨天、存档和撞车修复已获用户实机确认；物体表面光影闪烁也已确认修复，该问题在 PC 原版存在。它们不代表最新 NRO 的整体稳定性已经验收。主机 MoltenVK 跨视口原点的微小光栅差异仍保留为[单独诊断](reports/ending-raster-origin.md)，没有通过放宽阈值将其改为通过。

整体设计见 [架构与模块边界](docs/architecture.md)，各阶段当前状态见 [路线与收尾清单](docs/roadmap.md)。`reports/` 保存历次实现与验证记录，旧报告中的“尚未接应用”等描述只代表当时状态。

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
| 结局／特殊剧情 L + 右摇杆 | 按摇杆方向旋转镜头并显示对应提示图标，手动速度为最初的 6 倍 |
| 结局／特殊剧情 R + 右摇杆 | 右推拉近、左推拉远，上推升高、下推降低，并显示对应提示图标；手动速度为最初的 6 倍 |
| A | 对白推进 / 交互 / 菜单确认 |
| B | 游戏姿态切换；子菜单返回，SYSTEM 主界面返回游戏，确认框取消 |
| X / Y | 游戏镜头切换 / 拍照 |
| ZL | 慢走 |
| `+` | 游戏中打开暂停菜单；选人页确认当前角色；音量页恢复默认 MAX |
| `−` | 开关原版 FPS 显示，默认关闭，长按不重复切换 |

剧情文字界面按住 R 连续强制快进，松开即停止自动翻页；A 仍为单次推进。结局及特殊场景的 R＋右摇杆继续调整镜头。

主菜单、选人、存读档及游戏内 SYSTEM 界面的鼠标保持显示；左摇杆自由移动，ZL减速，十字键逐项选择，A确认。选人、存读档、鉴赏和相册目录可按 B 返回；照片或确认框中先关闭或取消当前层。音量页 B 保存并返回上一级，+ 恢复三项 MAX。SYSTEM 主界面按 B 恢复游戏，返回标题或退出的确认框按 B 取消。见[菜单快捷键验证](reports/menu-shortcuts.md)。

左侧黑边居中显示金色日文按键说明，仅保留操作内容，不显示标题或镜头倍率；中文说明留待用户要求更新中文补丁时切换。结局和特殊剧情另显示触屏提示：触屏定位光标，配合 A 确认；结局需要拖动操作目标时按住 A 滑动。音量页支持触屏直接点击和拖动。提示绘制在游戏截图之后，不写入相册照片，也不覆盖中间的4:3画面。

背景音乐、语音和音效均恢复原始播放基准，已撤销总输出和 BGM 的额外增益。新配置与音量页“恢复默认”均为三项 MAX（0 dB）；保留已保存的手动设置，不再导入原 PC 数据中的音量配置。见[音量默认值验证](reports/audio-defaults.md)。

默认打包当前游戏和诊断入口所需的 13 个 PP（含选人资源 `bk3_18.pp`、相册资源 `bk3_19.pp` 和天气资源 `bk3_20.pp`）、配套 TBL，以及 CKP/ATR/FAM/FTT 散文件；`--full-data` 可复制全部 22 个归档。所有复制文件校验 SHA-256，不打包 EXE、原存档或补丁。素材仅用于本地自有副本，不属于源码许可范围。历史 SD 包不会自动更新；新包须用 `--output` 指定独立目录。

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

原版分析与阶段证据见 [移植记录](reports/porting.md)，当前功能及验证范围以 [收尾清单](docs/roadmap.md) 为准。任务流程、音视频和存档已接入；应用当前不挂载汉化包。
