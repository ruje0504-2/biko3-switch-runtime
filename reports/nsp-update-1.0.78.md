2026-10-02：按用户要求，把现有汉化与去码合并补丁制作为本体更新 NSP，同时更新原版 NSP 与 NRO。文字修正仅在汉化启用时生效，见[中文流程文字修复](chinese-dialogue.md)。

文件为 `交付/biko3-01094F68D7333800-update.nsp`，2,994,387,680 字节，SHA256 `9297d1cc2b218ffafaf5f1eebbdaa8e6c3b7eaabb7e05889b240bcfdede16023`。安装本体 `biko3-01094F68D7333000.nsp` 后再安装此更新；无需另拷 NRO、Data 或 `patch.pp`。名称仍为「尾行3」、作者 `ILLUSION`、显示版本 `1.0.78`、图标来自 `icon.jpg`。

更新采用 CNMT Patch 类型 `0x81`，TitleID `01094F68D7333800`、内容版本 65536，扩展头关联本体 `01094F68D7333000`。Program、Control NCA、NPDM 与 NACP 存档 owner 保留本体 ID，Meta NCA 使用更新 ID。本体的 Application CNMT 指向同一个更新 ID。游戏进度及照片继续使用本体用户的 HOS 存档。

更新 RomFS 包含完整 168 个原版 Data 文件和根目录 `patch.pp`。这是完整资源更新，所以体积约 2.99 GB；没有制作 BKTR 二进制差分。仅把 `patch.pp` 放进普通更新 RomFS 会丢失所需的 Data，不能把这样的文件当作完整可用更新。[Atmosphère 文件系统驱动](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libstratosphere/source/fssystem/fssystem_nca_file_system_driver.cpp)在存在 indirect table 时才构建间接存储，本包使用完整 RomFS；这项格式判断来自源码，不代表已在 Switch 安装运行。

`tools/package_nsp.py --update-patch` 复用本体的 NSO、NPDM、NACP、图标及原始 Data 准备流程，用 [hacPack](https://github.com/DarkMatterCore/hacPack/tree/e506cb58b7843d86df7518156debd28f3b575638) 生成三个 NCA 和 NSP。CNMT 按[实际结构定义](https://github.com/DarkMatterCore/nxdumptool/blob/rewrite/include/core/cnmt.h)写入 Patch 扩展头、Program / Control 内容哈希与大小以及末尾 SHA256。密钥只在本机工具读取；游戏资源、二进制和密钥不进 Git。

成品独立提取检查见 [verification JSON](nsp-update-1.0.78-verification.json)：

- 三个 NCA 的 section / payload 哈希，以及 CNMT 中两个内容记录的 ID、大小、SHA256 全部通过。
- 168 个 Data 文件逐项匹配原始清单；唯一新增文件为 37,165,253 字节 `patch.pp`，摘要与既有优化补丁一致，`BKPT2` 标志 3 同时启用汉化和去码。
- 提取 NSO 与当前 ELF 独立转换结果一致，本体/更新包/NRO来自同一份程序；NPDM 程序 ID、NACP 名称/作者/版本、16 图标和存档 owner/容量均核对。
- 本体与更新关联、Patch 内容版本 65536、CNMT 类型和末尾摘要均核对；官方签名状态与自制软件本体相同，实际内容哈希通过。

本机未执行 Switch 安装或 HOS 更新加载。用户已明确后续实机自行验证，本轮不扩大剧情审核；本轮只本地提交，不推送 GitHub。
