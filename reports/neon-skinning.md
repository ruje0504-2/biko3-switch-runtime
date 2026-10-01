# ARM64 NEON 矩阵与 ENVL 蒙皮

2026-10-01。Switch 使用 NEON 并行计算矩阵组合、位置与法线。骨骼影响数组和父矩阵保留 `prfm pldl1keep` 内联汇编预取提示。默认主机仍走标量实现，ARM64 主机可通过 `-DBK_ARM64_FAST` 执行与 Switch 相同的优化代码。

## 实际 NEON 验证发现的修正

此前的主机/ASan 检查只执行标量代码，不能证明 NEON 数值正确。此次启用实际 NEON 后，固定 EXE 的齐次坐标第 1,032 号样本失败：原版输出 `2478.106201171875`，单精度 FMA 输出 `2478.090087890625`，超出既有 `3e-6` 误差限。原因是 W 列相消后的小数值被单精度累加扰动，再经除法放大。

`arm64_math.h` 现使用两路 double NEON，按原标量顺序累加 float 输入乘积，最后才转 float。矩阵别名、W 容差与除法、失败不改输出、法线直接线性变换规则保留。没有放宽对照阈值，也没有改 GPU 着色器。

新增 `arm64-math` CTest 在 ARM64 主机上始终启用 NEON，并在编译时拒绝退回标量。它保留了失败样本的原 EXE 结果，检查矩阵/点/法线、输入输出别名和 W=0 拒绝；旧单精度实现已复现断言失败，新实现普通/ASan 均通过。

## 验证

独立构建目录 `build/neon-host`、`build/neon-asan` 均设置 `CMAKE_C_FLAGS=-DBK_ARM64_FAST`，普通为 RelWithDebInfo，ASan/UBSan 为 Debug。对照固定 SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e` 的 EXE，资产取本地日文游戏数据。

- 两构建各 4 项 CTest：ARM64 数学、蒙皮、frame-tree 和 X pose。
- 两构建各 2,048 个齐次坐标样本、800 个完整蒙皮权重/次序样本；实际 NEON 与原指令误差为 0。
- 两构建各 80 个实际模型、1,645 个 ENVL、2,077 个蒙皮样本、947,457 个顶点对照，误差为 0；包括六个普通演员的四个动画采样姿态。
- 两构建各 212 次实际模型矩阵组合，误差为 0。普通构建另完成六演员 528 个完整姿态、142,296 个矩阵、11,040 次头部祖先姿态，误差为 0。
- ASan/UBSan 无诊断，关闭了 LeakSanitizer，不声称 LSan 通过。
- 指定 Mesa NVK 交叉构建通过，NRO 16,240,696 字节，SHA-256 `d8873a4ea515ba90a14ad4b22b99327b0b2b25a32d05d2548c1834357e9f230e`，ELF 未解析符号为 0。反汇编确认使用 `.2d` 双精度向量 FMA 与预取指令。

摘要在 `reports/neon-conformance-verification.json`，原始日志在 `local/neon-conformance/`。可用现有 `original_skin_weights_oracle.py --homogeneous`、`original_skin_oracle.py`、`original_matrix_oracle.py` 配合 `BK3_BUILD_DIR` 重跑对应构建；ASan Python 使用已有的预链接 launcher 和虚拟环境包路径。

## 性能与范围

Apple M4 上相同 `-O2` 的标量/NEON 函数微基准，交替顺序各 7 轮，排除首轮后中位数：矩阵乘法 4.944→3.932 ns/次（约减少 20%）；点变换 1.306→1.303 ns/次，未测到有意义的改善。两边结果摘要相同。此结果只测主机函数，不能外推为 Switch 游戏帧率提升或预取的独立收益。

本版尚无新的 Switch 实机性能/画面验收；完整自然剧情和完整移植仍未完成。只更新构建目录 NRO，不整包。本轮一次 GitHub 推送此前已完成，此次修正仅本地提交。
