# media 模块契约（M5尚未整体验收）

当前新增`avi/avi_clock/avi_surface`，见`reports/avi-texture.md`。CPU拥有RIFF/MSV1/帧时序与明确的像素政策，不依赖Vulkan/libnx。scene/avi_texture拥有解码器、可复用RGBA与GPU纹理，借用renderer；不能让媒体线程修改游戏状态。FFmpeg仅用于主机对照，不链接运行时。

以下PCM说明含早期阶段快照：当前audio为64槽，已有保位暂停/恢复，Switch audout已使用独立线程；以`reports/prop-interaction.md`和`reports/switch-performance.md`为准，不能沿用旧主线程泵送/不支持暂停的结论。

`pcm` 已实现有所有权的RIFF/WAVE PCM16单/双声道解码。原始字节可立即释放，采样率不强制归一；不执行文件I/O或native调用。全部2239原WAV已独立解码比较，包含22000/22090 Hz。

`pcm_mix` 是无状态CPU线性重采样、循环位置与音量/pan处理。输入时间为该声音开始后的**输出采样帧**：提交队列的混音位置与设备真实已播放位置由后端分别提供，不能混用。整数有理数相位避免逐帧浮点累计漂移；所有声音先累加到float立体声，再统一限幅/量化。此可移植DSP策略不宣称与某个Windows声卡驱动逐样本相同。

`audio`已实现32个独立声音槽、有所有权的引用计数clip、混音队列和已播放游标。命令在下一个未提交帧生效，同一边界的重播/音量命令合并；已提交PCM保持不变。每个槽保存尚未消费的版本，口型查询会读正在播放的旧版本，直到设备消费越过新命令边界。替换音频、释放调用者引用不会让队列或口型读悬空内存。当前支持重播、循环、音量/pan和释放；暂停/恢复/任意seek尚未提供，不借用clear伪装暂停。

`platform/audio_switch`接入libnx audout，48kHz双声道PCM16，4个10ms缓冲，内存按4096字节对齐。仅回收完毕的缓冲推进已消费计数（10ms粒度下界），不按墙钟估算或把提交量当播放量；关闭服务后才释放缓冲。主线程泵队列，长帧可能耗尽40ms预缓冲，统计queue_drains但不宣称连续播放无断音。设备错误为本次会话的硬失败；没有假静音成功。主机应用明确保持无声，`audio-probe`是可控离线消费器。

`scene/npc_audio`连接7个真实脚步音、指定语音资源、原PCM220样本窗口和共享口型包络。`bk_npc_frame_tail`按脚步→网格阴影放置→语音/表现阶段组装，仍需上游真实空间/流程状态。Switch角色资源检查场景R播放明确指定的PH10101.wav，用已消费游标更新口型；这不构成游戏对话分派。

离线输出不等于设备播放成功。Horizon输出适配放platform，视频纹理上传由render承接。解码器不调用GPU/native API，不从媒体线程直接修改游戏状态。解码失败、队列结束和播放完成必须区分。Switch硬件声音、延迟和长帧连续性尚待验证。

当前构建目标`bk_media`依赖资源服务、公共编译选项和libm，无额外第三方解码库。PCM验证见`reports/pcm-audio.md`；视频当前只支持`avi-texture.md`明确的MSV1子集，不将它当作任意AVI支持。
