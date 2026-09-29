# 结局表现组件与语音采样验证记录

2026-09-29。汇总现有 `4df6c0` 组合表现和语音包络组件的已取得证据，并记录本次测试入口登记后的实际运行。新组合表现组件尚未替换生产 `frame_invoke` 的通用推进；以下结果不表示父控制器或自然结局完成。

## 调度对照范围

`local/original-ending-presentation-oracle.json` 记录固定EXE上的12,000帧、216,307次服务调用、1,715个失败前缀与2,999次可变回调，最大误差0。动画、材质、BOM、表情和音频叶操作在这个对照中是显式观察服务；它验证顺序、参数与回调后的实时别名读取，不能代替资源叶实现或自然结局验收。

所有原地址针对 SHA-256 为 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e` 的固定中文EXE。真实组件探针使用日文资源，不执行原Windows音频驱动。

## 语音边界与实际消费游标

`local/original-ending-voice-oracle.json` 记录原 `4ad363/4ad5a4` 的12,000帧对照，包括3,437个有效PCM窗口、6,381个非播放状态、3,901次播放期间采样失败和480次原子拒绝，最大误差0。额外2,714组解码PCM边界检查覆盖442字节环形缓冲、440次跨尾窗口、894次过短缓冲拒绝和1,348次尾部/游标拒绝；未将原32位容量比较的边界改成越界读取。

真实资源生成的消费记录已通过原函数回放：1,800帧、682,000字节采样PCM、150个非播放状态，最大误差0。证据在 `local/ending-presentation-boundary-host.verification.json`。这使用真实混音器消费游标，不以墙钟代替声音位置，也不是Switch实际声音验收。

## 当前普通/ASan检查

`test-host.sh` 现在构建所需CPU ASan目标，并执行普通/ASan `ending-presentation-probe` 后逐字节比较采样记录。新增运行已实际执行且均退出0：十种配置、每种构建1,800帧，voice=1,730、manual=1,790、rejected=10；几何FNV为 `220d15e79017ac61`，PCM FNV为 `6edf82aec69ed451`。日志为 `local/ending-registered-presentation-host.log` 和 `local/ending-registered-presentation-asan.log`。

两份记录各892,808字节，逐字节相同，SHA-256均为 `fe4117431762f4e55262d044165d576065eb350e749b98cf98a2aa907dc2d092`，也与此前边界对照记录一致。记录位于 `build/ending-presentation-host.pcmtrace` 和 `build/asan/ending-presentation-asan.pcmtrace`，不加入源码Git。ASan使用 `detect_leaks=0`，不宣称通过LSan。

本次补充控制器源码读取被自动检查拒绝，未执行，因此没有宣称已经完成父控制器接入或完整资源加载尾部。未通过改写阶段值、增加通用动画推进或跳过镜头/语音来伪造完成。自然结局、其它分支、后续剧情与持久化仍待恢复和验证；没有新增Switch实机测试。
