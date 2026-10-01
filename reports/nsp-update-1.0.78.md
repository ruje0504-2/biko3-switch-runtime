2026-10-02：按用户最终要求，重打本体和真正的 BKTR 差分更新。旧的 `01094F68D7333000` / `01094F68D7333800` 及约 2.99 GB 完整资源更新包已被本次成品替代。

| 项目 | 本体 | BKTR 更新 |
| --- | --- | --- |
| 文件 | `biko3-01094F68D7330000.nsp` | `biko3-01094F68D7330800-update.nsp` |
| TitleID | `01094F68D7330000` | `01094F68D7330800` |
| 显示版本 | `1.0.78` | `1.0.78cn` |
| CNMT 类型 / 内容版本 | Application / 0 | Patch / 131072 |
| 字节 | 2,957,146,840 | 49,003,232 |

先安装新 ID 本体，再安装对应更新。名称「尾行3」、作者 `ILLUSION`、图标 `icon.jpg` 不变。更新自动启用合并汉化与去码；独立 NRO 保持 1.0.78，不需要随 NSP 安装。用户最终决定保留 2 GiB 存档＋64 MiB journal。新 TitleID 的 HOS 存档与旧 `…3000` 分开，不自动迁移旧进度。

BKTR Program NCA 的 RomFS 段使用 `CRYPT_BKTR=4`，有 3 条间接映射和 1 条 generation-zero AES-CTR-Ex 条目。虚拟镜像 2,988,539,904 字节，其中 2,945,457,344 字节（含文件间填充）来自本体；更新只携带 43,082,560 字节实际差异及对齐/两张表，物理段 43,155,456 字节。原始 168 个 Data 文件均从本体复用，新增 `patch.pp` 为既有 37,165,253 字节 BKPT2，内容没有重新修改。

`tools/package_bktr_update.py` 以完整更新作为本地中间输入，对实际 RomFS 文件位置及字节进行比较，生成映射。两张表每张 32 KiB，三段分别映射新校验/目录前缀、本体 Data、含合并补丁的后缀。新 IVFC 头来自目标虚拟镜像，不能直接沿用未加补丁的本体 IVFC。`tools/hacpack-bktr.patch` 让 hacPack 接受生成好的 BKTR 段，继续使用原有签名、key area 与 CTR 加密实现。

CNMT 含 156 字节 PatchMetaExtendedData：一条本体历史头，以及本体 Program、Control、Meta 三条内容信息。Program / Control / NPDM / NACP 存档 owner 都使用本体 ID，只有更新 Meta 使用 `…0800`。Control 从实际本体提取，仅改显示版本为 `1.0.78cn`。格式依据为参考工程的 [BKTR 记录](../reference/kisaku-switch-runtime/reports/update-nsp-bktr.md)、[nxdumptool 结构](https://github.com/DarkMatterCore/nxdumptool/blob/rewrite/include/core/bktr.h)与 [hactool 解读实现](https://github.com/SciresM/hactool/blob/master/nca.c)。

独立成品检查见 [verification JSON](nsp-update-1.0.78-verification.json)：

- 从最终 NSP 重新提取三个 NCA，使用 hactool 的 `--basenca` 实际合并本体与 BKTR，Program 的全部六层 IVFC 校验通过。
- 合并结果恰好为 168 个原版 Data 文件加 `patch.pp`；逐文件大小和 SHA256 与原始清单/现有补丁一致，补丁标志为 3。
- NCA section/content 哈希、CNMT 内容记录、156 字节本体历史、两个 TitleID 的关联及版本均通过；生产 CNMT 尾部按参考格式为零。
- NSO 与已验证当前 ELF 的转换结果相同，NPDM ID/范围正确；NACP 除显示版本外逐字节匹配本体，16 个图标和存档容量相同。
- 这是打包改动，没有修改运行时、重新扩大剧情测试，或将主机校验写成 Switch 安装/启动验证。官方签名状态仍与自制软件本体相同，实际内容/完整性哈希通过。

本体 SHA256：`85b4cf4706fd43284ab35dff0e78986334a6eb9b7ebecbbfcbf8b5978f428df6`。
更新 SHA256：`d3a6af50bef874684000001b517c932d45dafc59606e4eb7876399a40a156b7e`。
本轮只本地提交，不推送 GitHub；ZIP 打包仍按用户的暂停指令保留。
