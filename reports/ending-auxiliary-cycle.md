# 自动片段切换与真实资源对照

2026-09-29。补齐此前自动控制器 `4965b9` 的验证记录；相关待确认会话均已取得退出0的终态，并按新登记的测试入口重新执行普通/ASan资源检查。原地址只适用于固定中文EXE：SHA-256 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。运行资源为现有日文数据。

## 生产调用与所有权

`scene/ending_normal_session` 的 `BK_ENDING_AUXILIARY_4965B9` 已调用 `bk_ending_auxiliary_tick_apply`，不再把当前模式每帧重新提交给手动模式选择入口。自动控制器使用实际片段预测、共享随机状态与保留倒计时；手动模式选择保留独立接口。

实际资源探针检查预测接口与当前实例的 duration/end/source/rate 一致，自动控制器不提前发布姿态，不改变兄弟实例的播放配置、时钟或缓存。应用持有跨结局重建的 cycle，场景借用该对象。最新应用随机状态衔接的单独验证见 `ending-random-state.md`。

## 原指令对照

`local/original-ending-auxiliary-tick-oracle.json` 记录完整控制器12,000帧、41,874次服务、678个失败前缀和1,146次可变回调。请求13/14分别覆盖1,722/1,774次，源时间回写6,536次，覆盖256种倒计时值；逐字节一致、最大误差0。该层的片段请求、声音、眼纹理与随机服务是观察边界，不能据此单独声称真实资源叶服务等价。

`original_ending_auxiliary_cycle_assets_oracle.py` 的待确认会话现已取得终态：五个真实主模型、1,800帧、691,840份片段时钟快照和4,222,386份矩阵，最大相对误差0。设置阶段执行10次真实手动模式改变，随后由原自动控制器、片段请求、随机数、SRT及层级发布推进；请求13/14分别发生3/5次。该夹具观察声音调用且不绑定可选眼纹理目标，真实音频/眼资源由下面的独立检查覆盖。结果在 `local/original-ending-auxiliary-cycle-assets-oracle.json`。

## 实际资源与内存检查

新增到 `test-host.sh` 的两条 `ending-auxiliary-probe DATA --cycle` 已在普通和 CPU ASan/UBSan 构建执行，均退出0。每种构建覆盖五套模型、30种声音配置、3,600帧、21次自动切换及30次眼选择；矩阵FNV为 `4b26a1818a4a1f60`，6,339,840份PCM采样FNV为 `a104554b5ca98779`。最终倒计时12，共享随机状态 `b0652f6c`，兄弟实例与未发布缓存保持。

当前成功日志为 `local/ending-registered-cycle-host.log`、`local/ending-registered-cycle-asan.log`。旧 `local/ending-auxiliary-cycle-host.log` 仍保留修正前的覆盖断言失败，不能把旧日志替换为新成功记录或继续当作当前构建的终态。当前探针使用同一 seconds 推进演员并进行自动预测，切换次数门槛仍为大于10。

这些结果证明该组件及所覆盖资源路径，不证明父阶段的自然进入、其它角色控制器、全部结局或 Switch 实机行为。完整移植继续保持未完成；交付目录与依赖锁未变。
