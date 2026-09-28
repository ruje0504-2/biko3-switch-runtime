# AVI视频纹理与选择页特殊外观

`media/avi`解码原bk3_18的poi.avi和D_moza.avi：单视频流、MSVC/CRAM、RGB555、256×256、30fps、90帧。RIFF范围内严格检查块、偶数字节填充、嵌套rec、idx1大小/偏移/关键帧；保留PP范围外填充。00db仍按流的压缩格式处理，不能当未压缩帧。限制256MiB/2048×2048/65536帧；其他流/编码明确失败。

解码器借用不可变AVI，自持两幅CPU图；块支持单色、双色、八色和保留前帧的跳块。输出顶到底、RGB555高位清零。跳转从前一关键帧重建；失败不发布半帧，旧像素/索引保留。源字节在open后可以释放。

两份真实视频180帧、各300次顺序/逆序/随机请求，与FFmpeg9.0.1的有效RGB555位逐像素一致。普通/ASan直接PP探针540次请求、35,389,440像素hash d480bc55fd18db88一致。合成测试包含不同块、行方向、跨块skip、嵌套rec、损坏帧后的独立关键帧、原子失败、截断和6000变异。FFmpeg只是主机参考，不进入Switch链接；来源记录在THIRD_PARTY.md。

`media/avi_clock`恢复521d78/521ed2。clock535dcc来自进程FILETIME差值的毫秒，按signed32转float秒；独立于733700游戏计时和GetTickCount面板时钟。时差和帧率先分别落float，乘积在x87精度下转换；534398是signed64截断再取EAX低32，不能改成signed32截断。初始last_frame=0，首帧0保持旧纹理；越过length重新取clock，取start+1；紧接着可能请求0。帧号在解码/Lock之前锁存，失败不在同一索引重试。

固定中文EXE的完整521d78/521ed2共18,000次对照，clock/索引/调用顺序误差0；4367循环、9113真实原复制分支执行。VFW及DirectDraw接口是显式夹具，未替代原x87和复制循环。原525579执行确认format5为X1R5G5B5。format7实际上为P8，原51fa7b请求7后还依赖设备协商/回退，不能推测固定Windows输出格式。

`media/avi_surface`明确采用可移植表面政策：紧凑40字节、正高度RGB555 DIB，保持原source+44与直接逐行复制，因此跳过底行前两像素、跨行延续，最后两个原越界位置填黑。选择原支持的format5，RGBA8按归一化颜色转换；另覆盖原565转换的green低位0。原9113复制结果的273390个定义内像素一致，18226个尾像素按显式黑色政策验证。创建后未播放时为不透明黑色，原未初始化纹理不作像素等价承诺。

这一DIB政策与[Microsoft的正高度RGB规则](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)及Wine的紧凑输出布局相符，但没有Windows VFW实际返回缓冲的证明；不可将其说成完整原版视频画面复刻。原版相关指令仍固定于既有中文EXE，不宣称日文保护EXE逐指令证明。

`render/bk_texture_update`在活动帧外复制到可复用暂存缓冲，等待前一帧读完；下一begin在绘制前统一记录布局屏障/传输。多次更新合并最后一幅，不更换GPU图像、描述符、采样器或排序ID；销毁会解除待上传链。81帧/40回读、在途复用、合并、待上传删除、截图恢复和非法更新检查普通/ASan像素误差0。真实AVI纹理480帧、474变化/28保持请求普通/ASan同RGBA hash ec300bf52ca640e1。

`scene/selection_render`支持显式movie clock的61构造，拥有真实poi.avi并绑定五个演员各自D_moza.bmp。`actor_render_texture_surface`只替换GPU表面，保留原文件的alpha提示及纹理对象排序身份，借用资源随旧绘制快照存活。`selection_session`按原抽签决定60/61，不重抽或替代；在51ac5d指定位置更新视频，替换后旧GPU/演员/视频一起延迟回收。视频时钟由独立signed毫秒字段输入。

25个61世界/镜头组合600GPU帧、20跨换人旧快照普通/ASan一致，hash db504d48d5b7835a。另200个冻结姿态/灯光的诊断镜头帧和25个恢复帧，单独推进视频观察到365像素变化（组0/3/4）；组1/2没有在这些视角观察到变化，五个绑定均实际构造，不能宣称所有材质都在原镜头可见。原脸部特写不能充当视频可见性检查。

完整UI/世界/语音/视频会话394帧，其中229帧61演员，8次换人、2次无副作用重绘、2次实际目标请求；普通/ASan RGBA f1eb56b43d6f4ca0、PCM6dac0477cade1bf2一致，截图逐字节相同。正常60的600世界帧/394会话帧保持此前RGBA及PCM校验值。4个针对性CTest、2架构检查和指定NVK构建通过，源码NRO854a416f…。ASan关闭LeakSanitizer。

原标题→38→flow8仍未接应用；special1镜头另缺，flow8实际资源/对话/菜单及后续目的地仍需恢复。不能把此组件当作完整移植完成或Switch实机验收。交付FPS包f530d486…保持冻结，Mac防休眠继续。
