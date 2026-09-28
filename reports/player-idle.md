# 开场玩家更新与镜头距离衔接

实现4c1313：先401b0a请求玩家live idle action，然后复制position到previous，使用保留的vertical(+2a8)、水平velocity和turn输入执行4b39d9全場墙面/地面，按原yaw生成root placement，最后清interaction_mode(+579)。不做速度积分、不重算身高、不改阴影位置、不执行stance/interaction。game/player_idle_step只规划CPU，scene/entry_assets_step_player_idle先请求真实XAN再执行物理/root；后续失败终止帧，不回滚已发出的clip request。

新增actor_pose_request直接调用现有request逻辑，不推进调度器、不刷新局部/世界缓存；0秒advance仍会改变first-step状态，不能替代它。纯请求、重复请求、非法请求保持和后续首次推进均有主机/ASan单元检查。

补充4bdc12原controller+43c：NPC当前原点到上次发布Cam_AUTO的XZ距离。follow_camera_step_distance在成功时返回这一float；旧接口复用相同实现。entry_assets_step_camera_distance仅FOLLOW分支更新它，handover/hold保留；调用者直接传共享player scene camera_distance。phase1 orbit先用它平滑+440，再将+43c重置40供后续碰撞修正。

验证：

- 完整原4c1313、401b0a、4b39d9及矩阵root写入，全部29实际ATR/模型执行928次。828定义良好几何组逐项一致，最大相对误差1.25448e-15；100原退化边存在未初始化投影的组单列，继续沿用明确的跳过首个零长度边端点修正策略。568墙命中、565地面名称。仅资源构造/头屏幕投影为边界。
- 原4bdc12+XAN+SRT+发布720步，对比新距离输出、完整镜头pose和时钟，最大误差0；545步错误提前发布Cam_AUTO会分歧。
- 45真实entry及对应background collision，1080开场帧、97200个保持的子节点local/world矩阵、45次follow到handover再到player orbit衔接，普通/ASan一致。player root、previous、重复idle请求不推进和阴影位置保留都通过。NPC在该探针为真实资产的固定实例，不声称完整AI中央帧。
- 50主机CTest、28Python、42CPUASan通过；NVK交叉构建undefined symbols0，NRO 568e69be91c1398ea8f59b382b3277a1dd49ff5af0a1636f30c88037974985d3。SD归档仍0.3.5，应用仍诊断入口，未宣称完整可玩或真机验证。

中央51a682/4ec78d还需把AI/路点音效和共享状态绑定进完整帧；开场phase0/2到1还包含51a190里的对话/UI、相机stage结束和字体/消息切换，不能只改phase字节。继续必要依赖，Mac防休眠保持。
