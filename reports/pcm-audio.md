# PCM、播放队列与 NPC 音频连接

本阶段接通 NPC 帧依赖的 WAV、混音、已消费游标、脚步/语音包络及 Switch audout。不是完整音视频阶段或游戏移植验收；未获得 Switch 真机声音证据。SD 归档仍为此前 0.3.5。

## 实现

- `media/pcm`严格解析有所有权的 RIFF PCM16 单/双声道；保留原采样率，原字节可立即释放。未知块、奇数填充、格式/块对齐及整数边界均检查。原资产包括22000/22090 Hz，不能一律按22050处理。
- `pcm_mix`使用整数有理数时间线、线性插值、循环边界插值和 DirectSound 音量/pan 单位。32声音累加到float后统一限幅/量化。DSP为明确的可移植策略，不宣称与Windows声卡驱动逐样本相同。[音量](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418150%28v%3Dvs.85%29)和[pan](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708938%28v%3Dvs.85%29)使用微软定义的衰减方向。
- `media/audio`持有clip引用和声音版本。play/gain/clear在下个未提交帧生效，同边界命令合并；旧版本保留到消费越过替换边界。混音读提交位置，口型读消费位置。只有sink成功接收才递增提交量；倒退/越界消费时钟及设备错误终止会话。clear是释放声音，不伪装暂停；尚无暂停/恢复/任意seek。
- `platform/audio_switch`对接[libnx audout](https://github.com/switchbrew/libnx/blob/master/nx/source/services/audout.c)：48kHz双声道PCM16，4×480帧缓冲，每块4096字节对齐。回收缓冲后才能复用；消费计数为已完成缓冲的10ms粒度下界。退出先停/关服务，再释放队列内存。主线程填队列，长帧仍可能耗尽40ms预缓冲，记录queue_drains；无墙钟补算或丢失音频假播放。
- `scene/npc_audio`预载7个脚步PCM，按已验证事件顺序重播；解码语音后才替换旧声音。消费游标转换回源采样帧，以原声道交错顺序取220个PCM16样本（末尾回绕），驱动既有共享包络。音量衰减不影响原始包络窗口。`bk_npc_frame_tail`组合脚步→可选网格阴影放置→语音/表现阶段；失败中止该帧，不宣称全局回滚。
- 应用在Switch打开真实输出、更新前poll、更新后fill。角色资源检查场景R显式试听`bk3_06/PH10101.wav`，口型来自消费游标；不是推断出的剧情语音。主机应用仍明确无声，离线probe可以控制消费。场景切换清除声音，退出按所有权顺序释放。

## 验证

| 检查 | 结果 |
| --- | --- |
| 所有原WAV vs Python标准wave | 2239文件、304349609源采样帧，元数据和每个PCM字节一致 |
| 全WAV拥有权/ASan | 原输入释放后遍历全部采样；正常/内存检查hash均为15248069de5411d7 |
| 独立有理数混音单测 | 2520个32帧块，7种采样率、单/双声道、循环/非循环、极大时钟、音量/pan；分块不改变结果 |
| 队列/平台接口单测 | 旧音频版本、提前替换、循环、增益不重启、清除、引用释放、饱和混音、失败及重复生命周期；libnx模拟检查对齐/拷贝/回收次序/服务关闭前所有权 |
| 真实素材离线录制独立审计 | 10文件、46命令、673440输出帧／1346880采样，误差0；1400次游标/包络一致，含18次待生效变更、112次无可用窗口、85次循环后位置及1次队列耗尽 |
| 原资产 NPC 帧尾组合 | 5入口×160帧，真实语音、脸部、阴影；正常/ASan通过 |
| 应用角色场景→Vulkan | 240步、48回读；23张有声/静音角色图像存在嘴部差异，最大35像素；排除不同的状态条，初始主体逐像素一致。正常/GPU sanitizer输出相同 |
| 集成检查 | 31主机CTest、26 Python单测、25 CPU sanitizer CTest全部通过 |
| Switch NVK交叉构建 | 成功；未定义符号0；NRO SHA256 `8ccf2f402b9cd4dfbdbed8c21142e5da772fb38974c33fdcb42fba3938670e9b` |

离线WAV SHA256：`aca167bfe7236943db9bc0ab17fcc4442810c42b34c5ef59264be27e00cb1c03`。正常/ASan录制WAV和JSONL游标轨迹逐字节一致。原生PCM窗口与包络规则此前的EXE隔离对照见`face-controller.md`，脚步分派原指令对照见`footsteps.md`；本阶段未执行完整Windows音频驱动。

复现：`./test-host.sh`；`build/audio-probe local/game/MAINDIR/Data local/audio-offline.wav >local/audio-offline.jsonl`；`local/venv/bin/python tests/audit_audio_queue.py local/game/MAINDIR/Data local/audio-offline.jsonl local/audio-offline.wav`；`build/audio-render-probe local/game/MAINDIR/Data local/audio-render.rgba`；`./build-switch.sh`。精确输入/源码/日志/产物摘要在`pcm-audio-verification.json`。

## 后续

继续玩家输入、墙面滑动和场景/主流程装配；还缺BGM/剧情语音分派、暂停/seek、视频、存档与完整流程。需设备可用后检查 audout 真机声音、延迟、长帧及睡眠/恢复；libnx接口模拟和Mac MoltenVK图形检查不能代替这些证据。NVK版本固定Mesa26.2.2，Mac防休眠保持。
