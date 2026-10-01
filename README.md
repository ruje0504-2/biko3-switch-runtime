# 尾行3 / Biko3 — Switch 原生移植工程

**当前为版本 1.0.78，主要游戏功能已接入，可选汉化与去码外挂。** 使用 Nintendo Switch 原生输入、音频和 Vulkan NVK 渲染，画面保持 4:3，在 1280×720 输出中居中显示为 960×720。

已接通原标题、五人选择、开场剧情、游戏跟踪、失败与重试、暂停与返回标题；结局、特殊场景、鉴赏回放、音量设置和相册均有实际应用入口。游戏支持动态角色、跟随镜头、碰撞、雨雪、HUD 与拍照，存读档、下一段剧情交接、动作记录、解锁、照片和音量配置也已接入。

本次工程收尾交付包含 NRO、原版直装 NSP 和汉化与去码更新 NSP。后续 Switch 安装、运行与稳定性由用户自行验证，按反馈继续维护；已有主机检查按各报告的范围记录，不等同于实机验收。默认使用日文原始资源；NRO 加入 `game/patch.pp`，或 NSP 安装更新包，可启用原汉化、去码资源及中文操作说明。

## 最近更新

- **中文流程文字**：修复开场空行被误算为滚动、额外消耗 A 键的问题；中文对白、流程开场/失败/道具文字放大 30%，按完整行显示。仅加载汉化时启用，日文及菜单、按键说明不变；共用文字纹理复用以减少换页重复工作。[修复与验证](reports/chinese-dialogue.md)
- **汉化加载优化**：逐资源压缩，完整替换时跳过原资源读取；外挂由 79.1 MB 缩至 37.2 MB，109 项汉化图片、文本和字库的载荷读取量减少约 78%，全部 203 项最终资源不变。需同时更新 NRO 与 `patch.pp`。[优化与验证](reports/patch-loading.md)
- **原版直装 NSP**：名称「尾行3」、作者 `ILLUSION`、版本 `1.0.78`、TitleID `01094F68D7333000`，内置完整日文 Data，未装入补丁。游戏内照片及进度写入所选用户的 HOS 存档。[打包与存储验证](reports/nsp-1.0.78.md)
- **汉化与去码更新 NSP**：Patch TitleID `01094F68D7333800`，包含新版程序及合并补丁，安装在本体之上，共用同一个 HOS 存档。采用完整资源更新格式，约 2.99 GB。[更新包检查](reports/nsp-update-1.0.78.md)

- **减少渲染重复提交**：缓存同一命令缓冲中的图形状态绑定；固定测试片段中，绑定调用从 2,291,089 次降至 1,247,714 次，减少约 **45.5%**，画面和照片保持一致。[验证记录](reports/vulkan-bindings.md)
- **减少 NPC 无效遮挡计算**：视野外或已经确认被遮挡后停止继续检查视线三角形，地面高度与行为仍正常更新。[验证记录](reports/npc-occlusion-performance.md)
- **修复藏身退出卡住**：共用退出逻辑在动画结束时将水平位置归回进入点，修复剑道少女第五关藏身物边角退出后无法移动的问题。[验证记录](reports/hiding-exit.md)
- **修复相册与补充操作**：修复 Switch 相册黑底和照片打不开；剧情中按住 R 强制快进，FPS 默认关闭，可用 − 切换。[验证记录](reports/album-switch-fix.md)

以上调用量变化不是总 CPU 用量或实机 FPS 的降幅。Switch 矩阵与 CPU 蒙皮也已使用 NEON intrinsics 和少量内联汇编预取，并完成 ARM64 原版数值对照；具体范围见 [NEON 验证](reports/neon-skinning.md)。最新 Switch 性能收益尚未测量。

NRO 名称为 `biko3-runtime`，作者字段为 `ILLUSION`，版本为 `1.0.78`，使用本地 `icon.jpg` 生成的图标。当前本地构建为 `build-switch/biko3-runtime.nro`，16,408,236 字节，ELF 未解析符号为 0；SHA256：

```text
11afda078af0f9d0479b83057d1469e708ada317ea790326552fba1327ac9fb9
```

本次增量位于 `交付/汉化与去码外挂-20261001`，包含新版 NRO 和外挂；独立 NRO 在 `交付/biko3-runtime.nro`，原版直装包在 `交付/biko3-01094F68D7333000.nsp`，更新包在 `交付/biko3-01094F68D7333800-update.nsp`。旧的 `交付/SD卡根目录` 是历史包，不会随源码或 GitHub 推送自动更新。本次新外挂使用 BKPT2，需要与新版 NRO 一起更新，原 Data 无需重拷。

《尾行3》是 Direct3D 时代的 3D 游戏，与《鬼作》的 AI6WIN 不同。本项目参考 [kisaku-switch-runtime](https://github.com/ruje0504-2/kisaku-switch-runtime) 的本地素材隔离、主机验证和 Switch 打包方式。

渲染器直接静态链接 [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) 的 NVK，使用 `VK_NN_vi_surface` / `NWindow`。当前锁定提交 `5dba7886c56460ff47c3038e323d07b9547d6212`，Mesa 26.2.2；启动日志记录设备名称和 driver ID，并校验 Switch 使用 NVK。依赖版本见 [锁定文件](config/dependencies.lock.json)。

## 验证与当前范围

本批先完成普通与 ASan 各 203 项补丁资源和 10 项存储检查；封包优化时的汉化开场 1,100 帧与旧补丁逐字节一致。随后文字修复各检查 121 段真实文本 / 242 个裁切、465 帧中文 UI、150 帧日文文字和中日流程提示，另运行 1,100 帧实际开场查看放大后的画面。架构检查和指定 Mesa NVK 交叉构建通过；最终两个 NSP 提取后逐项验证原版 Data、NSO、NACP、NPDM、更新关联和内容哈希，更新包另核对合并补丁。各阶段范围见上方报告。

此前渲染优化前后及 ASan 版本各回放 1,054 个游戏帧，最终画面和两张照片逐字节一致；NPC 查询与原版的 1,392 组结果一致；藏身退出修复通过同点边角复现及实际应用回放。这些是各次改动的历史证据，未在本轮扩展重跑剧情。

此前雨天、存档和撞车修复已获用户实机确认；物体表面光影闪烁也已确认修复，该问题在 PC 原版存在。它们不代表最新 NRO 的整体稳定性已经验收。主机 MoltenVK 跨视口原点的微小光栅差异仍保留为[单独诊断](reports/ending-raster-origin.md)，没有通过放宽阈值将其改为通过。

整体设计见 [架构与模块边界](docs/architecture.md)，各阶段当前状态见 [路线与收尾清单](docs/roadmap.md)。`reports/` 保存历次实现与验证记录，旧报告中的“尚未接应用”等描述只代表当时状态。

## 构建

需要 CMake 3.24+、现有 devkitPro/devkitA64、libnx、Switch expat/zstd/zlib、Python 3.12+（带 Pillow）以及 glslangValidator。版本和下载摘要统一在 `config/dependencies.lock.json`；依赖放在本项目 `local/`，不会替换系统的旧 Mesa。

```sh
python3 tools/fetch_sdk.py
# 将自己的正方形图标放在 icon.jpg，构建时自动转换为 256×256 JPEG。
./build-switch.sh
```

图标素材保留在本地，不随源码分发；可用 CMake 的 `BK_NRO_ICON` 指定其他源文件。图标转换使用 CMake 选中的 Python/Pillow；本机为 `local/venv/bin/python`，更换环境可用 `-DPython3_EXECUTABLE=/path/to/python` 指定。

官方 CI SDK 的静态库没有带齐 Rust 标准库。构建使用与 SDK 完全一致的 `nightly-2026-09-23` AArch64 标准库；下载包校验 SHA-256。GitHub Actions 附件有保留期限，建议保留已验证的 `local/mesa-sdk.zip` 与 `local/rust-std.tar.xz`。附件过期时脚本明确失败，不会静默升级依赖。

主机资源检查：`./build-host.sh`。macOS Vulkan 检查还需 MoltenVK：`./build-host.sh --vulkan`。`MOLTENVK_PREFIX` 可指定其安装前缀。

主机使用离屏验证入口，可指定帧数和场景：`build/biko3-preview local/game/MAINDIR build/office.rgba --scene office --frames 16`。省略 `--scene` 默认 `game`，首屏为原版标题；显式 `--scene title` 保留旧标题素材检查。`BK_DEVELOPMENT_ENTRY=1` 仅供旧开发流程回归。主机入口不是桌面可玩窗口。生成目录为 `build/`、`build-switch/`；无需素材的合成测试可直接运行 `./test-host.sh`。

## 原版素材

原始 `尾行3 [汉化]/` 保持只读。安装包及 W02 同目录，离线提取命令：

```sh
python3 tools/prepare_game.py '尾行3 [汉化]/安装/biko3.EXE'
python3 tools/bk3_assets.py local/game/MAINDIR/Data --json local/resource-index.json
```

提取器输出已存在时不覆盖，可传 `--output` 使用新目录。本次已经提取完成，不必重跑。提取及资源读取不启动 Windows 游戏。基础数据使用日文原始资源与 Type_S/G 字库，外挂在读取时覆盖相应资源；原版存档不会自动导入或覆盖。流程分析仍以固定摘要的可分析 EXE 为依据，日文安装 EXE 的保护壳尚未解出，证据限制见存读档报告。

## 汉化与去码外挂

将本次增量交付中的 `switch` 文件夹合并到 SD 卡根目录，同时更新 NRO 和 `switch/biko3/game/patch.pp`；安装新文件名后移除 SD 卡同目录中旧的 `biko3-preview.nro`。保留已有 `game/Data`，无需重新复制完整游戏。外挂为 37,165,253 字节（BKPT2），启用汉化界面、剧情、字库、中文按键说明及原去码补丁；退出后移除 `patch.pp`，下次启动恢复日文与原版效果。旧 NRO 不支持 BKPT2，新 NRO 仍支持旧 BKPT1。

从自有原补丁重新生成：

```sh
python3 tools/prepare_combined_patch.py \
  '尾行3 [汉化]/去码汉化补丁' local/game/MAINDIR \
  local/patch-speed/patch.pp
```

输出外挂及资源摘要，不运行 Windows 安装器、不改写原数据。输出已存在时不覆盖，重新生成可指定新路径。原补丁署名/协议随本地交付保留，补丁素材不在源码仓库中。实现与检查范围见[合并补丁记录](reports/combined-patch.md)和[加载优化](reports/patch-loading.md)。

## 原版直装 NSP

本地成品 `交付/biko3-01094F68D7333000.nsp`，2,957,146,840 字节（约 2.96 GB），SHA256：

```text
dd8eb121782f37e915342139c152b98d862e4e6a95c4e888e7fb25e39232941e
```

安装后从 HOME 进入，名称「尾行3」、作者 `ILLUSION`、版本 `1.0.78`，图标取自 `icon.jpg`。TitleID 为 `01094F68D7333000`。包内含全部 168 个原版日文 Data 文件，不含汉化或去码补丁，也不依赖 SD 上的 NRO 与 Data 目录。

游戏内 Y 键 / 拍照图标生成的照片随当前用户存档保存在 HOS SaveData，和游戏进度、音量等一起管理；配置为 2 GiB 存档及 64 MiB journal。此处的照片仍由游戏相册查看。旧 NRO 的 SD 存档 / 照片不自动迁入；继续运行 NRO 时仍使用旧 SD 路径。

若另行启用外挂，Atmosphere 的 RomFS 覆盖位置是 `atmosphere/contents/01094F68D7333000/romfs/patch.pp`，使用同一份优化版文件；这不改变 NSP 本体。该方式依赖用户的 LayeredFS 配置，未新增实机验证。实现依据见 [Atmosphere 变更记录](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/changelog.md)。

也可安装 `交付/biko3-01094F68D7333800-update.nsp`，直接启用汉化与去码，无需另放外挂文件。它是关联上述本体的 Patch 更新（内容版本 65536），显示版本仍为 1.0.78；包含完整资源，大小 2,994,387,680 字节，SHA256 `9297d1cc2b218ffafaf5f1eebbdaa8e6c3b7eaabb7e05889b240bcfdede16023`。安装顺序为本体、更新包。游戏存档仍属于本体 ID；安装核对与格式说明见[更新包记录](reports/nsp-update-1.0.78.md)。

重新打包使用已有 devkitPro 工具、hacBrewPack 和用户本地密钥，不下载或分发密钥：

```sh
local/venv/bin/python tools/package_nsp.py local/game/MAINDIR \
  --output '交付/biko3-01094F68D7333000.nsp'
```

输出已存在时不覆盖，可换新输出路径。打包脚本生成同名 JSON 清单；实际成品核对见[NSP 记录](reports/nsp-1.0.78.md)。

更新包另使用本地 [hacPack](https://github.com/DarkMatterCore/hacPack)（本次构建提交 `e506cb58b7843d86df7518156debd28f3b575638`），默认可执行路径 `local/hacpack/hacpack`，可用 `--hacpack` 指定：

```sh
local/venv/bin/python tools/package_nsp.py local/game/MAINDIR \
  --update-patch local/patch-speed/patch.pp \
  --output '交付/biko3-01094F68D7333800-update.nsp'
```

## SD 卡预览

```sh
python3 tools/package_sd.py local/game/MAINDIR
```

以**完整内存的应用模式**打开新构建的 `switch/biko3/biko3-runtime.nro`。默认进入原版标题，Switch 版不再创建、写入或轮换 `.log` 文件；发生错误时仍在屏幕显示原因，可按 B 返回。照片和临时暂停截图写入 `sdmc:/switch/biko3` 下的独立输出目录。

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

左侧黑边居中显示金色按键说明，仅保留操作内容，不显示标题或镜头倍率；默认日文，加载汉化外挂时自动切换中文。结局和特殊剧情另显示触屏提示：触屏定位光标，配合 A 确认；结局需要拖动操作目标时按住 A 滑动。音量页支持触屏直接点击和拖动。提示绘制在游戏截图之后，不写入相册照片，也不覆盖中间的4:3画面。

背景音乐、语音和音效均恢复原始播放基准，已撤销总输出和 BGM 的额外增益。新配置与音量页“恢复默认”均为三项 MAX（0 dB）；保留已保存的手动设置，不再导入原 PC 数据中的音量配置。见[音量默认值验证](reports/audio-defaults.md)。

默认打包当前游戏和诊断入口所需的 21 个 PP（`bk3_00` 至 `bk3_20`，除 `bk3_17`，另含 `fambom`），包含结局、特殊场景、选人、相册和天气资源；同时复制配套 TBL，以及 CKP/ATR/FAM/FTT 散文件。`--full-data` 可复制全部 22 个归档。所有复制文件校验 SHA-256，不打包 EXE、原存档或补丁。素材仅用于本地自有副本，不属于源码许可范围。历史 SD 包不会自动更新；新包须用 `--output` 指定独立目录。

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

原版分析与阶段证据见 [移植记录](reports/porting.md)，当前功能及验证范围以 [收尾清单](docs/roadmap.md) 为准。任务流程、音视频和存档已接入；汉化与去码可通过上述可选外挂启用。
