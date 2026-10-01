# Vulkan 图形状态绑定缓存（2026-10-01）

跟踪画面的每个模型部件原来都重新提交 pipeline、纹理、灯光和顶点/索引缓冲绑定，邻接部件使用相同状态时也重复调用。现在仅在绑定改变时提交；新帧和截图恢复后的新命令缓冲重新建立绑定。改动仅在 Vulkan 后端。

所有图形管线继续共用原 layout，绘制顺序、矩阵推送、灯光内容和同步不变。计算管线使用独立绑定点，不会覆盖图形绑定；每次重新录制命令缓冲时则必须清除缓存。这与 [Vulkan descriptor 绑定规则](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindDescriptorSets.html)和[命令缓冲状态规则](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)一致。

用原有 `game-session-probe` 做固定渲染输入对照，包含 1054 个游戏帧、两次照片和暂停/恢复。计数器只加在本地测量副本，生产后端不增加日志或采样开销。

| 调用 | 原实现 | 缓存后 |
|---|---:|---:|
| 图形 pipeline 绑定 | 460,227 | 70,794 |
| 纹理 descriptor 绑定 | 459,035 | 307,560 |
| 灯光 descriptor 绑定 | 452,565 | 1,056 |
| 顶点缓冲绑定 | 460,227 | 434,152 |
| 索引缓冲绑定 | 459,035 | 434,152 |
| 合计 | 2,291,089 | 1,247,714 |

绑定调用减少 **45.5%**，绘制仍为 459,035 次。原实现、新实现和新实现 ASan/UBSan 的最终画面及两张照片逐字节相同；最终 RGBA SHA256 为 `8bb0f73890feb710a0cd545254b2d691be0a3920c4bf9d5c321f6cc8dd5d9cf1`。这是主机工作量减少，不是 Switch 帧率或总 CPU 用量的降幅。

相关验证已完成：普通和 ASan/UBSan 各 4 项 Vulkan 检查（截图继续绘制、像素、灯光、动态网格）；图形/计算切换探针各 628,276 个分量检查通过；2 项架构检查通过。未扩展剧情路线审核。

指定 Mesa NVK 构建通过，ELF 未解析符号 0。新 `build-switch/biko3-preview.nro` 为 16,318,520 字节，SHA256 `4009c0af715316c769d697ab6d776fd7f1caa187bd571e923931010e76d77715`。本轮仅本地提交，不推送、不整包，SD 未写入；新版本的实机 FPS 尚未测量。

计数、摘要与验证范围见 `vulkan-bindings-verification.json`；本地测量代码与日志在 `local/render-bindings-work/`。
