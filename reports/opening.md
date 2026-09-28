# 开场对话、面板与镜头交接

scene/opening_phase实现4ec528/4ec6c0/4ec777及外层phase=1写入；opening_session连接真实脚本、共享物品提示状态、se004点击音、字体所有者和真实camera slot0。原帧先51a682更新，再世界遍历，最后51a190开场逻辑。阶段0读取本次面板步进之前的stage；正文结束后面板退完且镜头slot0.source>=end，才开始镜头交接。stage1显示玩家，stage2进入操作；phase3直接准备物品字体而不写NPC行为或重新选镜头。分支锁定旧phase，即使改为1，本帧仍执行原开场分支的UI。

shared panel复用item_notice_state，notice_visible为同一状态。开场ma_05提示采用enter1/exit1/idle11：stage2同帧进入alpha往返变化，保留direction字节，构造后request1。开场面板和提示位于原1280坐标，字体Type_S.FTT使用104,380,432,72和16/18步长；进入操作后同一所有者重建192,400,316,268和16/16物品布局。item_notice_render拥有同一面板、提示、图标和字体，按旧phase只提交该分支的元素。

scene/dialogue_assets持有原始脚本，资源重载保留标签和pending媒体状态；517ba9关闭只清规定字段，保留CR及C/F/E/M和媒体标志。item_feedback接入同一metadata结构并提供4ec87c重载，仍与开场dialogue分开；不擅自消费开场脚本的媒体标签。system_audio支持4e6dee原8个槽se000..006/se099，显式保留不同voice和加载音量，开场点击为槽4。字体/资源或音频失败中止当前会话，不把部分副作用视为可重试事务。

验证：7200组实际51a190开场分支（截至51a3bc），原parser/close/panel/prompt执行，620次实际关闭；另6000组原提示脉冲。对话、共享状态、字体/音效/镜头边界调用顺序和文本绘制门限一致。45真实入口共2927帧自然进入phase1，含flow8/38开场、普通phase2及area8 phase3，未手动改阶段；随后各运行8操作帧。真实FTT82次上传；普通/ASan均产生5619840 PCM样本，5479240非零，hash c39ec8b4443233cd。屏幕投影、时钟和推进按键仍是明确的测试输入；原有24帧手选phase测试作为调度覆盖保留。

Vulkan普通/ASan各通过120开场帧和310物品帧，独立采样/混合比较14380800像素，最大误差1/255；已查看原素材输出。50host/28Python/42CPUASan通过。Switch Mesa NVK交叉构建通过，undefined0，NRO 1a4ed1b6a82210733a2b227072a7ef64cc92d3f08cd795b408da19b197f3cb63；应用未引用的新组件仍可能被裁剪，产物哈希不变不能代表已接入游戏应用。

本阶段尚未实现51a190公共HUD/黑屏转场、4cbca4/4cb902完整操作HUD、完整场景加载释放和菜单/存档/视频/应用注册。开场组合探针不是完整可玩游戏或Switch实机验证；SD归档仍0.3.5。继续公共HUD与场景切换，防休眠保持。
