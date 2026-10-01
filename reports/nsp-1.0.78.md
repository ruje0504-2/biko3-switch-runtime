2026-10-02 追加：以下成品已用[中文流程文字修复](chinese-dialogue.md)后的 ELF 重新打包、提取和校验；本体原版 Data 与 HOS 存储实现不变。下列存储单测为本批此前的结果，没有为对白改动扩大重跑。

2026-10-02：按用户指定，在汉化加载优化完成后制作不带补丁的原版直装 NSP。名称「尾行3」、作者字段 `ILLUSION`、版本 `1.0.78`、图标来自用户 `icon.jpg`；TitleID 按用户最终要求改为 `01094F68D7330000`（将此前的末尾 `3000` 改为 `0000`），相应更新 ID 为 `01094F68D7330800`。

`tools/package_nsp.py` 使用当前 ELF 生成原生 NSO，配套 NPDM / NACP、Program / Control / Meta NCA。完整 168 个原版日文 Data 文件（2,945,456,435 字节）放进 RomFS；不包含 `patch.pp`、Windows EXE、原存档或 macOS 杂项。无需依赖 SD 上的 NRO 或 Data 目录。版本与 Mesa 锁由既有构建清单检查，密钥、素材和二进制不进 Git。

NSO 启动时挂载自身 RomFS，读取 HOME 所选用户，调用 `IApplicationFunctions::EnsureSaveData`，再挂载该用户的本应用 SaveData。NACP 设置必须选用户、2 GiB 用户存档及 64 MiB journal。照片、进度、解锁、动作记录和音量使用共同根 `save:/biko3`，照片在其 `album/`，其余持久配置在 `save/`。游戏内 Y 键及拍照图标的照片写入这里；这不是 HOME 相册截图。

每次文件关闭并完成替换、照片删除或备份恢复后显式 `fsdevCommitDevice("save")`，错误正常向调用方返回。`save` 模块通过应用注入提交函数，未反向依赖平台层。退出时最后提交并卸载；NRO 继续用原 SD 路径。旧 NRO 的 SD 存档和照片不会自动搬入新的 HOS 存档。

协议依据：[libnx fs_dev.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/runtime/devices/fs_dev.h)、[EnsureSaveData](https://switchbrew.org/wiki/Applet_Manager_services#EnsureSaveData)。当前 libnx 无此命令的高层包装，因此调用已有公开 `serviceDispatchImpl`；按协议传入 128 位用户 ID、返回所需空间大小。

实际成品核对（不是只检查打包输入）：

- hactool 提取最终 NSP 的 3 个 NCA，三者 TitleID 一致；所有 section / payload 完整性哈希通过。
- Program RomFS 恰有 `Data/` 下 168 文件，每个大小与 SHA256 均与打包清单和原数据一致，无补丁。提取的 NSO 与当前 ELF 独立转换结果一致；NPDM ID 与允许范围相符。
- Control NACP 16 项名称 / 作者、版本、所选用户、存档 owner / 容量 / journal 检查通过；16 份图标均为生成图标的相同字节。
- CNMT 是 Application 本体（type `0x80`、内容版本 0）；Program / Control 的 ID、大小和 SHA256 均吻合，NCA 文件名与自身哈希吻合。
- hacBrewPack 的 CNMT 末尾 32 字节按[上游实现](https://github.com/pplatoon/hacBrewPack/blob/master/cnmt.c#L31-L35)为零。第一次自写检查误要求此处等于 CNMT 的 SHA256，已按工具格式纠正；仍逐项检查真实内容哈希及外层 Meta NCA 完整性。官方 fixed-key / ACID 签名为空，自制软件验证显示 FAIL；Program NCA 的 NPDM-key 签名和实际内容哈希为 GOOD，不能将两者混为一谈。
- 新 NRO 的 NRO0 / ASET / NACP / 图标重新回读通过，仍是 `biko3-runtime` / `ILLUSION` / `1.0.78`；指定 Mesa NVK，ELF 未解析符号 0。
- 普通与 ASan 各 10 项存储检查通过，包括照片写入后重开 / 删除、暂停图备份恢复、进度、提交失败和 SD 无操作路径。这是主机注入提交函数验证，不是 HOS 实机持久性测试。

最终 `交付/biko3-01094F68D7330000.nsp` 为 2,957,146,840 字节，SHA256 `85b4cf4706fd43284ab35dff0e78986334a6eb9b7ebecbbfcbf8b5978f428df6`。详细字段见 [verification JSON](nsp-1.0.78-verification.json)。后续安装、运行和照片跨重启保留由用户自行验证，按本次要求不再作为交付阻塞项。配套更新包现为显示版本 `1.0.78cn` 的 BKTR 差分包，见[更新包记录](nsp-update-1.0.78.md)。用户询问存档容量后明确决定维持 2 GiB，本轮未减少。旧 `01094F68D7333000` 属于另一应用 ID，旧 HOS 存档不会自动转入新 ID。本批只本地提交，未推送 GitHub。
