# 来源与依赖

- 参考：[kisaku-switch-runtime](https://github.com/ruje0504-2/kisaku-switch-runtime)，提交 `e77e7ef9dca666d2351be2afe74e40780c5c6a90`，GPL-2.0-or-later。参考平台构建/验证组织方式及 BKTR/补丁历史格式；未复制 AI6WIN 运行时代码。
- [hacPack](https://github.com/DarkMatterCore/hacPack)，提交 `e506cb58b7843d86df7518156debd28f3b575638`，GPL-2.0，配套 `tools/hacpack-bktr.patch` 沿用该许可。与 hacBrewPack / hactool 一起仅用于本地 NSP 打包/校验，不进入游戏程序；密钥与生成内容不进源码仓库。
- [Mesa Switch](https://github.com/danfromtico/mesa-switch)，提交 `5dba7886c56460ff47c3038e323d07b9547d6212`，26.2.2，MIT 及组件各自许可。SDK 来自该仓库 Actions run `35860399051` / artifact `10750582639`，归档 SHA-256 `230b777e49e6a7efa43b13897d7abc5eec54bea68ff1732e9ff1ef8c63b1adeb`。原许可位于参考仓库 `licenses/` 和对应源文件头。
- Rust nightly 2026-09-23 标准库（rustc `1.100.0-nightly 6bb1652a0`），MIT / Apache-2.0 及组件各自许可。用于补全 Mesa NAK/NIL 的链接依赖；保留下载发行包中的许可证。
- [libnx](https://github.com/switchbrew/libnx)、devkitPro/devkitA64 及 Switch portlibs：平台和构建依赖，各自许可。最终程序链接 libnx、libstdc++（运行库例外）、expat（MIT）、zstd（BSD / GPL 双许可中的 BSD）、zlib。
- [SB3Utility](https://github.com/HF-Alamar/SB3Utility)，提交 `0ebf55cd7c7d2fc80c5691ee85a3b1016c5b84b8`：查阅旧式 PP 索引格式，未复制实现。TBL 编码、索引与 OBJM 结构另从本地 EXE 汇编核对。
- [SB3Utility-ODF](https://github.com/enimaroah/SB3Utility)，本地参考提交 `f8878b02c264ab40183805f92bb73a982ec6b970`：查阅后续 ODF 的字段组织用于比较，未复制代码；尾行3 的网格/节点跨度与它不同，实际实现以原 EXE 和本地文件校验为准。
- [WiseUnpacker](https://github.com/mnadareski/WiseUnpacker) 3.0.0：仅作本地离线提取工具；不进入 Switch 程序。
- Pillow、Unicorn、pefile：仅测试和本地分析依赖，不进入 Switch 程序。MoltenVK：仅 macOS 主机 Vulkan 验证。
- [Noto Sans CJK JP](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf)：日文侧边按键说明使用其固定字符的16像素位图子集，© 2014–2021 Adobe，SIL OFL 1.1，许可见 [licenses/NotoSansCJK-OFL.txt](licenses/NotoSansCJK-OFL.txt)。原字体仅用于本地生成，不随运行程序加载。
- [FFmpeg MS Video 1 decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/msvideo1.c)（源文件 LGPL-2.1-or-later）：查阅块编码规则，作为自实现有界 MSV1 解码器的格式参考；未链接 libavcodec。FFmpeg 9.0.1 仅作主机逐像素对照工具。RIFF/AVI 容器另参考 [Microsoft AVI RIFF reference](https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference)。

原版游戏、中文补丁、模型、图片、音视频、字体和 Windows 程序归原权利人所有，不包含在源码许可中。`reference/`、`local/`、原版输入、构建结果和本地素材包均排除在 Git 源码外。
