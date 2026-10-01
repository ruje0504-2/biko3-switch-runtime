# 失败结算HUD与五种镜头依赖

`scene/failure_hud` 恢复完整51AFCB：确认按键别名由调用者解码；outcome6第一次确认选择各组10401/20401/30401/40401/50401并绑定文字、重置文字started/enabled，第二次确认启动黑幕。计数器保持原DWORD回绕，其他值不擅自重置。其他outcome确认直接启动黑幕。panel先推进后request，prompt先request后推进，文字仅在panel阶段2/3绘制；输出保留当时alpha。黑幕ready时按40→2释放，再schedule68/mode2，随后才清原失败相关字段、置NPC行为1和等待时长3000。服务失败保留原顺序已发生的副作用并终止。

`world/failure_camera` 恢复4BC919、4BCAB3、4BCCA9、4BD04F、4BD3ED。对应俯视玩家、玩家正面、NPC前/后、选中道具后方；使用真实分组偏移、0.5或1倍时间平滑，先按旧world位置瞄准，再写新平滑位置。俯视不改FOV，其余请求0.2。原完成条件逐轴比较带符号delta<=0.1，并非绝对距离；大幅负向移动也会立即返回完成。NPC组越界会读原未初始化locals，移植明确拒绝；未定义aim几何同样失败，不伪造矩阵。

原EXE隔离对照：HUD12,000帧，438次消息绑定、4,596次release调用，状态/绘制门控/alpha/副作用顺序一致；五种镜头12,000步、4,061次完成，世界矩阵与平滑位置最大相对误差0，sin/cos、瞄准、世界设置和完成分支均执行原指令，仅FOV设备服务替换。普通与ASan/UBSan两个单元测试通过，覆盖服务失败前缀、无定义输入原子拒绝、负向完成判据；架构检查与Switch构建通过。

应用失败流程已经接入 `play_session`。group0/area0 的普通和 ASan/UBSan `failure-flow-probe` 均以真实输入完成 `flow02→flow40→flow68→flow02`，再次失败后回到标题；整个 session 销毁后 GPU 分配回到 renderer 基线。日志分别为 `local/ending-third-natural-next/failure-chain-20261001-host/run.log` 与 `failure-chain-20261001-asan/run.log`。两构建首次失败类型分别为 3、2，第二次均为 3；随机种子来自运行时，因此不声称逐帧状态相同。

另一次 group0/item1 的 area2 读档后探索进入了 `flow40`，它只是失败入口证据，不能当作成功剧情继续。随后已单独完成 area2→area3 的成功保存与新进程读档，见 `reports/ending-natural-item-route-verification.json`。其他角色、Switch 实机和完整媒体验收仍未完成。
