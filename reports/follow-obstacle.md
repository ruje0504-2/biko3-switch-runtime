# 跟随镜头的真实场景障碍

恢复4b61e5：仅normal.Y<float(cos(1.047))的三角形，三条XZ边顺序查询，无高度/名称/道具kind过滤。原4aed7a未命中保留旧交点，原caller忽略返回值；每mesh清一次临时点，原点(0,0,0)为miss哨兵，距离相等仍接纳。比较NPC body到交点的XZ距离，更新controller+43c和+524，probe.Y使用查询相机Y；不因前一命中缩短后续线段。4bef54/4bed86使(0,6),(4,4),(4,5)跳过几何。

world/follow_camera_step_collision在从旧Cam_AUTO第一次平滑后查询真实静态+动态碰撞，再执行4*seconds的修正平滑，最终返回缩短后的distance和保留probe。scene/game_frame现在直接调用该路径并绑定shared player wall camera_distance/player_view.probe；已移除必需外部miss回调。其他跟随/交接API保留此前接口语义。失败原子保持镜头、clip和输出；中央帧失败仍清理碰撞后缀。

原4aed7a在部分轴向交线生成NaN。沿用可移植的显式修正策略：跳过这条退化边、累计singular_intersections，并保留此前有限交点，不把原NaN传播进镜头。此分支不宣称与Windows非有限状态等价。

验证：

- 完整原4b61e5及CRT/intersection：2896mesh查询，包含全部29ATR的124真实mesh；2596定义良好组，最大误差0。300个原非有限交线组单列。77原profile gate查询一致。
- 完整原4bdc12+真实m01_01碰撞+XAN/SRT/缓存发布720步，46帧真实缩短distance，镜头/时间轴/probe/distance最大误差0；545步错误提前发布会分歧。仅FOV与独立资源构造分配/查找为边界。
- 45真实入口中央循环1080帧，现使用真实follow collision；普通/ASan保持2073600PCM样本hashb28d69fe1e966e4a和512722发布访问。阶段切换与屏幕投影仍为显式夹具，不代表51a190已接入。
- 主机/ASan单元覆盖原点哨兵、无高度门槛、相等距离、末尾非法几何原子失败和轴向退化策略；50host/28Python/42CPUASan及NVK构建通过，undefined0，NRO 35acd685d75adff880ebe76444130132afa38914d3f21613893f595599cee44b。

继续真实入口状态初始化/保留字段与动作绑定，再51a190开场对话/UI及应用注册。当前应用仍诊断，SD归档仍0.3.5，无Switch实机验证；Mac防休眠保持。
