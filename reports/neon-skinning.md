# Switch ENVL 蒙皮 NEON 快路径

Switch 的 ARM64 构建现在为仍走 CPU 的 ENVL 蒙皮启用 NEON：位置和法线的矩阵乘法使用向量累加，骨骼影响数组在进入循环前发出 L1 预取提示。位置的齐次 W 检查、接近 1 的容差、double 除法和最终有限值检查保持原实现；法线继续使用原始线性 3×3 矩阵，不添加逆转置或归一化。

快路径只由 `BK_ARM64_NEON` 启用。主机、ASan 和 UBSan 仍编译标量代码，因此既有 CPU 对照不会因为优化改变。Vulkan GPU 蒙皮路径也不改变。

验证结果：

- `build/test-skin` 通过；
- `build/asan/test-skin` 通过；
- 主机和 ASan `skin-deformation` CTest 通过；
- Switch `biko3-preview` 交叉构建通过，NRO 为 16,240,696 字节，未解析符号数为 0；
- Switch `skin.c` 反汇编确认包含 `fmla` 与 `prfm pldl1keep`。

这项优化没有改变资源格式、CPU/GPU 所有权或渲染接口，也没有生成新的交付包或推送远端。
