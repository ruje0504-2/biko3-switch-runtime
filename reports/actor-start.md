# 原始XAN加载后选片

`bk_clip_player_create_authored_start`按原版加载→初始请求/强制选片顺序建立状态。保存的current可以是未启用槽，但只有实际切换到有效槽时才接受；旧槽source仍作为blend_from。重复请求不发生切换时继续要求完整合法的authored状态；越界编号、非法过渡起点和无效目标均拒绝。普通fresh和直接authored构造规则保持原样。

`model/playback_create_started`在尚未采样之前执行此构造，actor_pose使用它。解决了原始h02_60保存current/requested=3、h03_61保存40而对应槽未启用的问题；没有修改资源文件，也没有把过渡起点清零。

验证：26份实际演员XAN（bk3_01及bk3_14的_55）共17,712状态、17,280采样与原401b0a/401d24/4026fe逐值相等，恢复此前拒绝的两份文件。眼阶段组合对照已移除两个编号归零的夹具，15套全部原始XAN执行360步、995,955矩阵，误差仍不超过7.11e-15。原始文件摘要均记录。

主机27项CTest、23项Python、CPU sanitizer 21项通过；新增边界及6000次变异涵盖新构造路径。45入口在普通/ASan运行均通过。Switch NVK构建未定义符号0，NRO SHA-256 `5bdf2693de0be64692da99afadcaebd3b5ea775dc5557b9b1facc2711d462567`；Mesa仍固定5dba7886。未重新发布SD包或声称实机通过。

下一项已定位：NPC的+4附属对象实际是`kage_01.xan`网格阴影，图形设置0/1使用它，设置2另走投射阴影。空间阶段复制主体root并将Y加0.1；表现阶段同步隐藏并全速推进其XAN。其模型无ANIM，下一步补齐这种合法的无SRT轨道实例和阴影装配。证据见`actor-start-verification.json`，完整移植未结束，防休眠保持。
