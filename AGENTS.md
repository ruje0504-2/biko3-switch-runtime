2026-10-01 NEON实际ARM64验证已补齐并修正单精度相消偏差：矩阵/位置/法线改双精度两路向量累加，预取汇编保留；旧实现原齐次样本1032失败，新普通/ASan各2048坐标+800权重、80模型947457顶点、212矩阵组合误差0，4项CTest含强制NEON回归通过；普通六演员528姿态/142296矩阵/11040头部采样误差0。见 `reports/neon-skinning.md` 与 verification JSON。M4矩阵函数约20%减时、点变换无明显改善，不能当Switch帧率收益。指定NVK NRO d8873a4e…/16240696、nm0，仅构建目录；自然故事/完整移植/新实机未验收。一次GitHub已到6fbe62e，后续不追加推送；防休眠69780、本地提交、不整包。下条“NEON尚未验收”为检查前快照。

2026-10-01 当前自然道具链已维护并通过：group0/item1 真实拾取→area2 保存/独立读档→area3 成功交接保存/独立读档。`tests/check_natural_item_route.py` 普通/ASan 共8进程、各29229帧，库存01000、只读存档不变、GPU回基线，无sanitizer；见 `reports/ending-natural-item-route-verification.json`。入口area1与初始随机种子为夹具，之后不注入阶段/结果；不能称标题到结局全路线。此前continue进入失败不算成功已纠正；失败→重试→标题另在group0/area0普通/ASan通过。phase4显式库存矩阵当前每构建64002帧已完成交接，下文旧48679帧/未完成为历史。本轮一次GitHub推送已到6fbe62e，后续仅本地提交；防休眠当前69780，不整包。NEON此前主机验证实际仅标量，正在独立启用ARM64快路径做原版对照，尚不能称NEON数值验收通过。

2026-10-01 当前增量：固定 EXE 对照恢复 47DC79 state1/2。state1 读取 72210C open，不再误用 722100 pending；媒体忙/镜头模式继续走 idle 分支，按键只取低 AL；47CB41 区域命中与 481C2C 菜单布置拆为独立服务，state3 写入顺序恢复；state5/6/7 效果 presence 改接完整 ending mixer 槽 2..46。host/ASan state1 单测通过；五角色 item0/1 加 item2/255 的正式 inventory 普通/ASan 共24次通过，每构建48679帧/45727输入/60图标/24生命周期/12失败构造。见 `reports/ending-inventory.md` / `reports/ending-inventory-verification.json` 与 `build/validation/ending-runtime-fktzogzu/result.json`。当前 Mesa NVK NRO 为 `c556d24a…`、16,240,696 字节，ELF 未解析符号零；持有道具完整 phase4自然交互、自然拾取存档重载、Switch实机和完整移植仍未验收；不要把入口矩阵称为完整路线。上一条GitHub增量推送已完成，之后不自动推送；不整包，防休眠64008保持。

2026-10-01 背包71BCDC..E0已从app唯一pickup owner借入结局，修复恒零UI/分支和鉴赏第二/三字节生命周期；生产第三类禁用target跳过原未初始化hover语音，严格step保留。见 `reports/ending-inventory.md`。12实际应用普通/ASan配对各48679帧/45727输入/60图标/24生命周期入口/12失败构造通过；原256初始化/1024入口/65536释放前缀、严格12000 CPU帧、720兼容+1200定义帧、5单测/29Python通过。旧third-session测试已改为当前R键与正确方向，普通/ASan五角色第三类回归和选人回归均在独立补跑中通过，终态见verification JSON。指定NVK NRO feb7ac94…/16232504字节、nm0。自然无道具group0完整保存13955+回放13473帧另本地配对通过，不计正式五角色验收；持有道具phase4真实后续发现state1误用pending代替open、idle/低AL等差异，待恢复完整原指令，不能称完整第三类路线已通。接续local/ending-third-natural-next/next.md。用户本次授权增量推送一次，之后不自动推；不整包，防休眠64008保持。下文背包pending未接说明为历史。

2026-10-01 用户确认此前物体表面光影/阴影明暗闪烁**已修复**，并明确这是PC原版已有的BUG。已关闭该问题，见 `reports/lighting-surface-layers.md` / verification JSON。此结论来自用户反馈；不再把该闪烁列为待修复，也不将其扩展为完整移植或全场景验收。下文“仍无本版实机确认”为确认前历史快照。

2026-10-01 用户最终纠正：第一角色**第二**室外场景书店旁，闪的是物体表面光影；de369a84仍未修好。已定位m00_11透明明暗层pivot近零残差和原交换排序、共面阴影光栅竞争，见 `reports/lighting-surface-layers.md` / verification JSON。game_preview背景显式启用stable_layers，排序key绝对值<1e-4归零，仅排序兼容；全局透明batch稳定priority/distance/file序，原指令API保留。反色lit/no-write阴影bias常量2+斜率1/128，其余材质不加。最终8项普通/ASan配对同摘要：各12视角2520帧排序隔离旧最多97379像素跳变/最大RGB63，新0；6组360帧共面/前置/遮挡旧24/33/0错采样，新0；7920非共面、72光照、材质与1054游戏/135暂停通过，游戏RGBA一致。2400原flush/14378距离、29Python/NVK/nm0通过。新NRO2296739c…/16232504字节仅构建目录，仍无本版实机确认，不宣称完整移植/原版全画面通过。背包未验证diff与真实hover未初始化拒绝新缺口存local/ending-inventory-pending-20261001（优先于旧pending）；此前陈旧binary测试不算验证。防休眠64008保持，本地提交、不推送、不整包。下文第一场景范围为旧记录。

2026-10-01 用户视频确认旧深度修复仍闪，最新明确第一角色第一室外场景最明显、室内应该不闪，不能归因雨雪。已将反向深度移至投影阶段，以double保留视图/对象组合的深度行，真实actor/static入口接入；见 `reports/lighting-motion-transform.md` / verification JSON。普通/ASan相同：合成2880帧旧109056错像素/248变化、新0；第一场景m00_10实际11组墙/光片×2释放位置7920帧，旧移动/减速/固定错误6/8/0、新0。65×49低分辨率压力仍残留23→13错误，不能报所有光栅情况修好或视频实机已验证。各90背景720帧、45演员540帧、办公室、540结局事件、1054游戏/135暂停通过；3368结局视口仍完整3D11/12、RGB131，UI/矩阵/队列12/12。29Python/NVK、nm0；NRO de369a84…/16232504字节仅构建目录。无新实机；防休眠64008、本地提交、不推送、不整包。结局背包未验证修改已隔离至local/ending-inventory-pending/changes.patch及同名probe，恢复后继续；不要误称已测试/已进本轮二进制。下文687e9e24旧修复不能当成当前实机已好。

2026-10-01 用户反馈移动画面时所有光照闪烁，已修复一项共用深度精度问题，见 `reports/lighting-motion-depth.md` / verification JSON。Vulkan内部在顶点乘法前将深度行改W−Z，D32比较/清除同步反向；CPU原投影、镜头、灯光和4:3不变。固定照明/移动相机六模式4608帧：旧renderer1400832错误像素/1596覆盖变化，新普通/ASan均0；每构建10项GPU检查、120帧局部清深度、628276传递标量、90背景720帧及1700帧应用通过，29Python/NVK通过。当前完整三维对照由9/12改善为11/12，仍差mode2/1280x720、最大RGB131/255，不能报全视觉通过；UI和全部矩阵/队列12/12一致。NRO687e9e24…/16232504字节、nm0，仅构建目录，无新Switch实机确认。防休眠64008，本地提交、不推送、不整包；结局背包71BCDC与完整自然流程仍缺。

2026-10-01 保存成功后下一剧情退出已在主机复现并修复，见 `reports/checkpoint-next-story.md` / verification JSON。原area entry保留last_crossed，道具读取固定1024槽标志；新路线短于旧索引时不能误用有效节点边界。world新增有界flag-slot读取并保留尾部标志，正常移动边界不放宽。修复前group0/area3、group3/4/area8普通/ASan失败；修复后六项配对通过：每构建五角色34803帧/40保存/40开场，加5599帧覆盖存档与五角色重载；GPU分配回基线。原45表46080标志/360道具决策、2560区域状态与53路线通过；3普通3ASan单测、29Python、指定NVK通过。NRO51b9679a…/16232504字节、nm0，仅构建目录，无新增实机验收。自然任务、结局背包71BCDC与3D9/12缺口保持；防休眠64008，本地提交、不推送、不整包。接续local/record-early-next.md。

2026-09-30 普通五角色已用真实输入完成前段→选择→保存→独立进程回放，见 `reports/ending-natural-record.md` / verification JSON。发现并修复连续选择回放释放类型误判，场景回收实际selected owner并延迟旧快照，原CPU动作不变；三种原分派/释放指令对照通过。六项普通/ASan配对：自然各102783录制+114731回放帧/35重绘，独立variant1各14382+10507帧/7重绘，12次新进程回放；既有鉴赏164220、混合11054、选择39463帧三项配对通过，29Python及指定NVK通过。NRO c3493ae9…/16232504字节、nm0，仅构建目录。单角色variant1本地自然phase3→6另各3731帧通过，不计正式覆盖。新发现ui_item未借用pickup.collected[0]，下一项接71BCDC真实背包以恢复持有道具的phase4分支；完整故事入口、3D9/12与实机仍缺。防休眠64008，本地提交、不追加推送、不整包；接续local/record-early-next.md。

2026-09-30 选择阶段真实输入链已补齐验证，见 `reports/ending-record-input.md` / verification JSON。删除拖动起点片段/状态夹具，五角色两路线真实命中→mode0/1/2/6/4→结束保存→独立进程回放；同步60Hz普通/ASan各226938帧，50ms墙钟/60Hz逻辑各203770帧，两时钟保存文件相同。缺失/无终点的retained路线在资源加载前明确拒绝，不用当前count或另一lane代替。每构建5单测、29Python、原codec19800660字节及鉴赏40803帧配对通过；1110源码/45数据核对。NVK NRO34c34ac7…/16232504字节、nm0，仅构建目录。单角色前段真实phase1→2另在local探索普通/ASan各4419帧通过，尚未纳入可复用测试；完整自然入口、后续phase2/3/4/7、像素9/12、实机仍缺。防休眠64008，本地提交、不推送、不整包。接续local/record-early-next.md与record-persistence-next.md。

2026-09-30 结局动作记录持久化已接正常应用，见 `reports/record-storage.md` / verification JSON。save借用五组进程记录，600020字节原Gray完整编解码、600052字节CRC文件；启动解锁后读取，flow10停止前保存，结束对话另提交解锁。原写16/读17普通与ASan各19800660字节一致；五角色两路线各73659录制帧+66079独立进程回放帧、30重绘，20进程重载与文件/状态/PCM配对通过；每构建5单测、29Python、1110源码/45数据摘要一致。三个入口/阶段边界仍为显式夹具，完整自然故事未验收；旧解锁缺记录入口检查、完整3D9/12与实机仍缺。NVK NRO b4063eaf…/16232504字节、nm0，仅构建目录。此前一次授权已推到4a26eff，本批仅本地提交、不追加推送、不整包；防休眠64008保持。接续local/record-persistence-next.md；下文记录未持久化为历史。

2026-09-30 flow48实际鉴赏入口已接通，见 `reports/special-session.md` / verification JSON。special_session按UI/主体/媒体/灯光/相机顺序装配，逻辑退出后旧GPU快照延至下一tick回收；共享菜单/结局相机与过渡状态，修复内部鉴赏函数遗留副本。AVI使用独立进程毫秒、完成构造后取起点及仅循环时第二次读钟；原18,000次配对一致。最终9项普通/ASan配对：新应用各10入口15,309帧10照片53重绘、原鉴赏40,803帧、截图/游戏回归及原world900帧通过，10照片逐字节一致；每构建10单测、29Python通过，1118来源一致。NVK NRO c43a06ac…/16,228,408字节、nm0，仅构建目录。自然记录bk3_Gray保存/重载、完整3D9/12与实机仍缺；防休眠64008、本地提交、不推送、不整包。下一项local/record-persistence-next.md；下文flow48未接为历史。

2026-09-30 跨流程截图所有者与相册扫描已修复，见 `reports/capture-lifecycle.md` / verification JSON。PlaySession持有截图、入口仅借用，B53954每分派前锁存、照片输出时读活值；晚回收/失败构造不取消请求。4AF5E1真实目录计数普通/ASan各110组12064名称220重扫一致；应用各五角色4968帧五照片/五失败借用者、旧截图及暂停返回共四项配对通过。4普通4ASan、29Python、指定NVK通过，NROa720c826…/16183352字节、nm0。扫描API仍待4E29B0调用，完整flow48加载/释放/应用未接，action8仍拒绝；自然记录/像素9/12/实机缺口保持。防休眠64008、本地提交、不推送、不整包；完全访问never，终端直接执行。接续local/ending-gallery-next.md。

2026-09-30 flow48真实UI/GPU/系统音/截图相册组件已验证，见 `reports/special-ui-media.md` / verification JSON。三组终态共7项普通/ASan配对：新UI各960帧4178493像素分量最大1/255、6实际照片/66重绘/12黑幕原网格对照；原UI11812帧、鉴赏30914帧及应用40803帧通过。已纠正旧夹具1起始音槽错误，special现在0/2/3/5/7、gallery0/2/3/4，直接取原4E72FE加载表；旧音效报告不可作正确映射证据。6普通6ASan、29Python、指定NVK通过，NRO73abceb1…仅构建目录；新增special组件仅静态库，flow48入口仍拒绝。下一项完整4E29B0/4E42E4、722224共享语音、app注册及跨流程待截图/B53954活值生命周期；自然记录/完整像素9/12/实机仍缺。防休眠64008保持，本地提交、不推送、不整包；完全访问never，终端直接执行。

2026-09-30 flow48完整CPU界面已恢复51B617/4E4472、侧栏/计数/镜头预设/截图请求/退出及原构造段，见 `reports/special-ui.md` / verification JSON。普通/ASan各38构造58失败、11,812界面帧937,152顶点162,152服务/832失败前缀，以及构造后51B647的3,800帧一致。原ge_16.tga文件名前缀别名会改写71AD90计时/71ADA0/71ADA8/侧栏/序列，补充旧“loader仅清sequence”；4E6154首播放会清armed，30秒切换仍正常，无兼容复位。4普通4ASan、29Python、NVK通过；NRO仍328bd51b…，新UI仅静态库，未接真实图片/截图/应用。下一项真实UI/GPU/系统音/相册及完整loader/release/flow48注册；自然记录/像素9/12/实机缺口保持。防休眠64008、本地提交、不推送、不整包；完全访问never，不请求终端授权。

2026-09-30 flow48真实音频/视频/GPU已验证，见 `reports/special-media.md` / verification JSON。special_audio借四loop字节/共享包络，区分46435E重播与直接Play续播；special_render一个真实主体、g1/4的poi.avi纹理，旧快照延后退役。原4E418E传NULL，已修special灯光全group2；53F0F8实为lstrcmpA，旧忽略大小写oracle也已纠正。四组普通/ASan配对：900音频帧/1213440PCM标量/3失败；63灯光配置406灯光/16000计划；1800真实CPU帧179绘制159重绘128358GPU顶点，状态/PCM一致。6普通6ASan、29Python、指定NVK通过，NRO328bd51b…仅构建目录，新owner仍未进生产ELF。下一项4E4472 UI及完整加载/释放/应用；自然记录/完整像素9/12/实机缺口保持。防休眠64008、本地提交、不推送、不整包；完全访问never，不请求终端授权。

- 用户已授权范围内的终端命令直接执行，不重复索要终端权限；若工具实际拒绝，说明具体拦截原因，不把工具限制说成用户未授权。

2026-09-30 flow48真实CPU场景已装配，见 `reports/special-world-assets.md` / verification JSON。special_world拥有主体/普通FAM/眼/灯光/双轨并适配完整51B647，外部音频/视频/UI仍必需。原423B01递归写+240，仅禁止绘制，不能误用+70隐藏剪枝；森林和GPU快照已分离此标志。三组普通/ASan配对通过：各900真实帧/1,799,280矩阵/1,498,140面部顶点/25构造拒绝；160原森林帧/51,200标志检查；88合成GPU帧/8显隐快照。6普通/6ASan、29Python、指定NVK通过，NRO08fadc90…仅构建目录，新owner未进生产ELF。下一项真实声音/视频/GPU、4E4472 UI及应用生命周期；自然记录/完整像素9/12/实机缺口保持。防休眠64008、本地提交、不推送、不整包；完全访问never，不再问终端授权。

2026-09-30 flow48真实双轨道及四类镜头已恢复，见 `reports/special-camera-assets.md` / verification JSON。三组普通/ASan配对通过：五组日文主体焦点各1,200帧/3,433,626矩阵、768重复ANIM样本及既有flow10的6,000帧回归一致；h02_55的15重复目标按原文件顺序覆盖，不再拒绝。4BB82E忽略第三参数，必须读真实733700；第三组根旋转160为原始弧度。4普通/4ASan单测、29Python、指定NVK通过，NROe585e851…/16,179,256字节仅构建目录；新owner尚未进生产ELF。下一项完整4E29B0脸部/声音/视频/灯光与4E4472 UI/释放/应用，不能称flow48可玩。自然记录/像素9/12/实机缺口保持，防休眠64008、本地提交、不推送、不整包。下文“4BB82E未实现”为历史。

2026-09-30 鉴赏菜单已接实际应用flow18与全部selection0..6入口，见 `reports/gallery-application.md` / verification JSON。四组普通/ASan配对终态通过；各40,803帧/40入口/25图片/10回放返回/172重绘，显式保存表与结束标记记录夹具不等于自然故事录制。第五类独立背景缺口已按4CE7CB..4CE806补齐；左摇杆/方向键/触屏与闲置光标唤醒已接。8普通/8ASan单测、29Python、指定NVK通过；NRO dd27f0af…/16,175,160字节，新增入口已进ELF。下一项独立flow48（4E29B0/51B647/51B617/4E42E4）、自然记录持久化及解锁链；完整像素9/12与实机缺口保持。防休眠64008、本地提交、不推送、不整包。接续local/ending-gallery-next.md。下文“菜单未接应用”为历史快照。

2026-09-30 鉴赏菜单CPU和实际资源渲染已恢复，见 `reports/gallery-menu.md` / verification JSON。两组普通/ASan配对各30,914原指令帧、570,993绘制与38,270调用一致；真实日文资源各900帧/50图片/100重绘，像素最大1/255、PCM一致。6普通/6ASan及29Python、指定NVK通过。菜单尚未注册app/front_end/PlaySession，新增符号只在静态库，NRO仍25c5940d…；不是flow18可玩或完整移植验收。下一项实际前端owner、selection0/3/4公开生产入口、独立flow48（4EB913→4E29B0）、自然记录/解锁/返回链；防休眠64008、本地提交、不推送、不整包。接续local/ending-gallery-next.md。

# 协作约定

- 2026-09-30 flow48逐帧CPU已恢复51B647与五辅助函数，见 `reports/special-event-cpu.md` / verification JSON。普通/ASan各6,000随机、876边界、12,000连续CPU帧、357,045调用/1,067失败前缀与4,096直接包络对照一致；共享708878/7C不得另建owner。3普通/3ASan及29Python、指定NVK通过，NRO19778878…仅构建目录，新控制器仅在静态库、未进生产ELF。真实4E29B0资源/4BB82E镜头/4E4472界面/释放及应用仍缺，不能称flow48可玩。721B38=Back、71AF48=角色焦点；groups1/4的4E6138是视频更新。自然记录/像素9/12/实机缺口保持，防休眠64008、本地提交、不推送、不整包。接续local/ending-gallery-next.md。

- 2026-09-30 生产phase8真实回放与进程owner已接通，见 `reports/ending-gallery-session.md` / verification JSON。应用一次初始化BkEndingProcess，交互6DDE98与鉴赏6C7F78/6DDE52分离，725704与保存镜头继续共享。最终六项普通/ASan配对通过：显式记录路线各10入口164,220帧/25资源切换/332重绘，另混合11,054帧；52,702与39,463帧交互回归及原60加载960标量一致。原控制与真实XAN核对发现三项等待问题，链完成、短语音开场等待、快速循环跨界为明确兼容修正，不能混入严格原指令等价；4普通/4ASan单测、29 Python/NVK通过，NRO25c5940d…仅构建目录。下一项真正flow18鉴赏菜单（4E7671跳表确认4EB93B→4C59C0）、自然记录/解锁/返回链；本批终点是退出请求，仍非完整移植/像素9/12/实机验收。防休眠64008、不整包；用户仅授权本批结束增量推送一次，之后恢复不自动推送。接续见local/ending-gallery-next.md。下文生产phase8未接为旧快照。

- 2026-09-30 选择/辅助结局真实节点与取景已修正，见 `reports/ending-node-bindings.md` / verification JSON。719B40绑定实际A_okosi，719B48显隐与719B44分开，ID随当前森林退役；4D39E6原版目标是旧发布5/13/0，已删除误用第二类公式与根节点替代。日文groups1/2/4缺A_okosi、group2辅助缺OYU保持为空。最终四项普通/ASan配对及两项原指令对照通过，新探针各20入口9760帧10实际交互；另52,702/39,463帧回归一致。原目标4096组及各10资源90分量差0。4普通/1ASan单测、29 Python/NVK通过，NROab554dc8…仅构建目录。完整目标/生产phase8/自然结束/像素9/12/实机缺口保持，防休眠64008、本地提交、不推送、不整包。下一项进程owner时注意：交互selected.expression_override是6DDE98，鉴赏共享6C7F78/6DDE52必须另设，不能照旧local笔记误合并；已核对原指令和PE初值。

- 2026-09-30 选择阶段真实服务已修复：401F71动画重播、401B0A固定10tick手动切换、真实UI52闪光/53与54UV、直接Stop所有者检查、辅助拾取与延后表情请求。见 `reports/ending-selected-adapters.md` / verification JSON。最终五项普通/ASan配对通过；新探针各52,702帧/10入口/40手动切换，状态与PCM一致，原版各60加载960标量一致；29 Python与指定NVK通过。NRO9170952d…仅构建目录。下一项719B40锚点与719B48显隐节点分离、4D39E6真实5/13/0目标（已发现误用第二类公式），再实际手势、进程owner与生产phase8/自然结束；完整像素9/12和实机缺口保持。防休眠64008，本地提交、不推送、不整包。

- 2026-09-30 选择阶段实际入口/开场/菜单换场已修复，见 `reports/ending-selected-session.md` / verification JSON。4D1025使用活跃721ED8、写721EF0开场gate并保留721EE4；真实拾取、choices与无BOM绘制已绑定。普通/ASan各70入口/边界、39,463帧、40次菜单换场与220旧帧重绘一致，阶段/两类应用配对通过；原版各60加载960标量一致。首次总检查因校验脚本漏写资源名前导反斜杠失败，原记录保留；只修断言后对同一日志补跑通过，运行时/二进制未变。NVK NRO b09eb9d2…仅构建目录。phase8仅30资源边界夹具，48302B/48BCBB尚未接；选择手势/结束UI服务、辅助拾取/表情、进程owner与自然结束仍需继续。完整像素9/12与实机缺口保持，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-30 当前接续：完整488674第五类回放已恢复，六状态、三轮镜头、角色音效链和结束等待普通/ASan配对通过；见 `reports/ending-gallery-auxiliary.md`。各6,000随机、1,720边界、35,223连续CPU帧、23,970调用及563服务失败前缀一致；指定NVK与状态/入口/重载回归通过。五类CPU控制器齐备，生产48302B/48BCBB仍未接；下一项进程owner、实际服务、prepare_final和跨loader phase8装配，不能只放宽4D1025入口限制。NRO95e29691…仅构建目录，完整目标/像素9/12/实机缺口保持，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-30 当前接续：完整48758C第四类动作回放已恢复，借用普通回放片段字节、表现反向/表情字段和retained.final，无新增状态副本；见 `reports/ending-gallery-tertiary.md`。普通/ASan各6,000随机、1,082边界、7,015连续CPU帧、14,313调用及716失败前缀通过，指定NVK构建和状态/入口/重载回归通过。剩余488674及生产phase8装配；CPU夹具不是实际资产/PCM/实机验收。NRO0c9a3ca4…仅构建目录，完整目标未完成，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-30 当前接续：完整4855C9选择动作回放已恢复，描述符source/chain写入、两类镜头与原提前返回计时已对照；共享6D1BC0焦点纳入同一GalleryCameraState，见 `reports/ending-gallery-selected.md`。普通/ASan各6,000随机、1,302边界、27,312连续CPU帧、139,832调用及36写入失败前缀通过，第二类共享状态回归与指定NVK构建通过。剩余48758C/488674及生产phase8接入；这些CPU夹具不是实际资产/PCM/实机验收。NRO f2b4f5a9…仅构建目录，完整目标未完成，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-30 当前接续：完整4843AA第二类回放已恢复，共享镜头保存实例与实际BkEndingState绑定；普通/ASan各6,000随机、836边界、34,745连续CPU帧/89,895调用及嵌套47D9EE一致，见 `reports/ending-gallery-secondary.md`。6D1BD8/DC与6DDE54是毫秒回绕计时；descriptor+200是chain配置而非ended标志。剩余4855C9/48758C/488674与生产phase8接入；CPU夹具不等于实际资产/可玩验收。完整目标未完成，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-30 当前接续：完整483AF0普通动作回放及48C8C2语音助手已恢复，真实BkEndingState别名绑定；普通/ASan各6,000随机、420边界、264语音、33,840连续CPU帧及132,919调用一致，见 `reports/ending-gallery-normal.md`。原6D1BCC在此为浮点秒数位模式；动画结束为相等或unordered，不能改成>=。剩余4843AA/4855C9/48758C/488674和实际phase8接入，当前仅CPU服务夹具通过；完整目标未完成，防休眠64008保持，本地提交、不推送、不整包。

- 2026-09-29 用户反馈结局摇杆镜头两轴反向，已在 `scene/ending_normal_session` 的手动输入适配处同时取反 X/Y，保留 L/R 修饰键和原始6倍速度。R+右摇杆为右推拉近、左推拉远、上推升高、下推降低；L旋转方向也相对上一版反转。原版world相机数学、自动轨道、左摇杆光标和游戏跟踪输入不改。见 `reports/ending-camera-direction.md` / verification JSON：旧方向断言已复现失败，修复后普通/ASan各972方向/速率检查、3651真实开场帧、384图标夹具/96GPU绘制及确认取消回归通过，架构与指定NVK构建通过。NRO `0a9c6ee2…` 仅在构建目录；无新增实机验收、整包或推送。完整目标未完成，防休眠60608保持；下文六倍版正向motion数值为历史，不能据此恢复反向操作。

- 2026-09-29 用户再次要求结局手动镜头加速：当前为最初的6倍，即此前3倍版再翻倍。L+右摇杆旋转，R+右摇杆调整远近/高度；UI镜头提示与侧栏拖动判断也使用L/R，A/B继续确认/取消/交互，自动开场/预设轨道不加速。见 `reports/ending-camera-six-icons.md` / verification JSON。普通/ASan各真实group0三尺寸3651开场帧、108速度检查（30/60/120Hz）、384显式UI门限夹具/96次真实图标GPU绘制及应用确认取消回归通过，两架构/NVK构建通过；NRO f406978e…仅构建目录，无新实机/全量交付/推送。完整移植和原验收缺口仍未完成，防休眠60608保持。下文3倍和旧NRO记录为历史，不要恢复旧倍率。

- 2026-09-29 用户要求的结局镜头键位现为 L+右摇杆旋转、R+右摇杆调整远近/高度，手动速度为此前3倍；不要恢复A/B修饰键或加速自动开场轨道。见 `reports/ending-camera-controls.md` / verification JSON。五角色三尺寸的开场相机/投影摘要普通与ASan一致，每构建15入口19674帧、540输入检查通过；指定NVK NRO为71eb2527…，仅构建目录。原三维像素/完整流程/实机验收缺口保持，防休眠继续。

- 2026-09-29 最终图片纯UI与重载保留资源已接入，见 `reports/ending-final-image.md` / verification JSON。新具名 `ending-runtime-6csffcf0` 的10项普通/ASan配对终态通过；图片15入口9878帧180重绘、627030通道最大1/255、150拒绝及缺失/损坏各3例。释放图片的group0是原占位参数；第三类纯UI后材质登记仍须背景在前。旧3D在下一真实tick退役，值形式灯光/背景跨长图片阶段保留；新3D的一次初始蒙皮不能误判为旧图重绘推进。9项CTest两构建及架构2项通过，NVK NRO1d5755e0…/15953976字节、未解析0。4D1025仍明确未实现；继续其phase5/6及4D39E6/正式gallery/自然结束解锁，不能以图片显示替代完整链。生产3D像素仍9/12，4:3、防休眠60608、冻结交付保持，不推送不整包。下文旧NRO/纯UI未接为历史。

- 2026-09-29 第三类实际场景/应用已经接入，最新已结算证据见 `reports/ending-tertiary-session.md`。`ending-runtime-ze245hyv` 的14项与 `bom-dual-assets-_s__hz0e` 的5项普通/ASan配对均已终态通过；下文“第三类仅CPU”“故事kind1不可用”及 fb072ec6 NRO 是旧快照。实际第三类5入口11,001帧，状态19f98e3393e05b00/PCM7d030796141894f2；应用kind1独立进入4D2320。进程共享状态仅初始化一次，旧快照晚释放不得清理新状态/音频。当前NRO9fe6e443…在构建目录，未新增实机验收。新混合计时/效果检查独立记录，不能算入旧14/5项。继续最终图片纯UI生命周期、4D39E6/4D1025、gallery与自然结束/解锁；完整像素9/12、居中4:3、防休眠60608保持，不推送、不整包。

- 2026-09-29 第三类CPU父控制476720和表现479137见 `reports/ending-tertiary-cpu.md` / verification JSON。普通/ASan各12,000控制样本、9,000表现样本，22,155/186,829调用、1,391/2,008共享变更、514/819失败前缀及20/4边界拒绝通过，原状态误差0。状态2等待释放、3调用47811C；第三组双附属演员，非操作动画.6/.3，操作阶段必须真实4E18AD/4A9019。新父控制仅持有6AFD1C/20与54CCD4进程字段，其他别名借用；表现直接借用FaceState并共用6AFD04，资源重载不可重置进程状态。场景提供实际BOM绑定容量，不从偏移猜容量。尚未接4D2320资源或应用，服务为必需边界；不能将CPU原指令夹具当成真实资产/可玩分支。新 `check_ending_tertiary_cpu.py` 可成对复现，test-host.sh在BK3_ORIGINAL_EXE设置时调用；本轮没有运行整份历史入口。NVK静态库含新增符号，ELF未引用，NRO仍fb072ec6…、未解析0。继续4D2320及47811C/478EAB/479739/4DAC12/47CB41、4E18AD/4A9019真实绑定和后续loader；故事kind1不改接第二类。像素9/12、4:3、防休眠60608及冻结交付保持，不推送、不整包。

- 2026-09-29 当前结局阶段生命周期见 `reports/ending-stage-lifecycle.md` / verification JSON。4CF318与4D00FA父控制/表现/真实音频/UI/双视图已接session；背景独立owner，groups0/2/3/4跨阶段保留，group1按原规则重载。旧CPU/GPU/图片快照延迟到下一真实tick释放；背景在前的BOM源必须单独捕获，灯光继承设备状态但重建注册顺序。普通/ASan各五组自然往返47,587帧、10切换、30重绘同状态8435a0e4ac4c36dd/PCM59bd723db024384b；第二类完整CPU63,077帧、场景63,430帧、边界重载1,560帧也成对通过。新check_ending_runtime.py已接test-host.sh，按改动选择套件；本轮未运行整份历史入口。common_hud_initialize只做原版部分初始化，独立进程夹具须先清零，不能修改生产规则清掉保留字段。最新NVK NRO fb072ec6…，15,888,440字节、nm未解析0；没有实机/全量打包/推送。下一项4D2320及其控制器，再其他loader、gallery phase8、最终图片纯UI阶段和自然结束/解锁；故事kind1不等于4D00FA。完整像素仍9/12，整体未完成、防休眠60608保持。详细接续在local/ending-lifecycle-next.md；下文第二类仅CPU等描述为历史快照。

- 2026-09-29 第二类结局 CPU 资源见 `reports/ending-secondary-assets.md` / verification JSON：4D00FA 独立使用 bk3_09 主体，森林为主体0/轨道1,2/背景3，无辅助或BOM；保留 A_okosi 的非中点目标公式、A_kuch跟随、初始FOV .2和第二组m02_90。普通/ASan各4096原目标/10入口32895矩阵/72片段/390节点及4失败前缀、8背景重试通过，误差0；NVK编译、fresh nm0。尚未接生产入口，NRO仍042e93e3…；继续47A5D0/47D3CB、尾部进程状态/UI/媒体及真实绘制。像素隔离见 `reports/ending-raster-origin.md`：同原点12/12 RGB差0，生产仍9/12、最大131/255，未改生产视口或阈值。防休眠60608已复核保持，不推送、不整包，完整目标未完成。

- 2026-09-29 当前结局绘制事件见 `reports/ending-draw-events.md` / verification JSON：生产普通4CF318接入真实51C736/4D9733，按721ec4及speech1选择双视口/普通绘制与音量恢复。special viewport来自4E755E，960×720内容为(0,270,600,450)，加内容原点；719c5c由app进程持有。普通/ASan各4096原视口、19670真实输入帧/30模式切换、540专项绘制/音量帧、退出/1700应用帧通过。UI/所有矩阵与队列12/12一致；完整3D画面仅9/12，仍需定位，不放宽阈值。NVK NRO `042e93e3…` 仅在构建目录、未解析0；后续其他loader/父控制/自然结束解锁/实机仍缺。下文未接入特殊绘制与旧NRO为历史快照；防休眠60608保持，不推送、不整包。

- 2026-09-29 最新普通结局交互修复见 `reports/ending-skin-homogeneous.md`：第四组 target26 的 bone449 W=0.99999994 被严格仿射检查误拒绝，现按原522b0d的含等号±1e-5容差与条件XYZ/W修复CPU/GPU，法线/权重/原子失败保持。原2048位置+800完整蒙皮普通/ASan误差0；80真实模型GPU加112合成、6071688顶点及五组两画幅19110帧普通/ASan+UBSan通过，1700帧应用两构建通过。NVK NRO `8341ac09…` 仅在构建目录，未解析0。原父控制/表现已实际接入，旧报告未接入说明为历史；特殊事件绘制、其他loader、自然结局/解锁、完整像素与实机仍未完成。防休眠60608保持，不推送、不整包。

- 2026-09-29 最新取景修复见 `reports/ending-framing.md`：已复现零目标导致头部Y=-4.4927，接入实际加载目标后回到-0.7772；公共镜头使用旧发布0/5/13节点，AUTO使用A_kuch，UI读取活动相机local。两种构建各五角色/10入口1390帧、50按钮、20相机调用及退出/RNG回归通过；组1/2的原缺节点路径采用显式跳过相关命中线段策略。画幅UI差0、全部actor矩阵/队列12/12一致，完整三维像素仅8/12、最大33/255，仍未通过整体验收。NVK NRO `2883f1c5…` 仅在构建目录；额外隔离副本故障注入、交付复制与机器可读报告整理两条命令被自动安全检查拦截，未执行，未新建交付目录/报告JSON。正向终态和构建校验JSON在local。真实入场父控制器/其他loader/自然结局仍缺，防休眠60608保持，不推送、不整包。

- 2026-09-29 当前接续见 `reports/ending-draw-stage.md`：普通结局按真实phase分派模型，视频移到绘制事件准备，phase9继续、phase7和纯重绘保持。普通/ASan各3392批次检查、136弹窗视频变化、24显式phase7空回读及退出/共享RNG回归通过；UI像素差0，原1080p三维边缘差异仍未通过。指定NVK NRO `ad230b01…`，只更新构建目录。六leave对照已由 `ending-clock-camera.md` 补齐，随机状态/自动cycle见 `ending-random-state.md` / `ending-auxiliary-cycle.md`，下文较早“原指令未执行”是历史快照。三组补充源码读取本轮被自动检查拒绝，未执行；完整父控制器、特殊接管与自然结局仍未完成，不将常规绘制修复视作整体验收。防休眠60608保持，不推送、不整包。

- 2026-09-28 用户确认「雨天 实机正常」，继续授权工程收尾。该反馈仅确认雨天效果，不扩展为雪天、完整结局或整体移植验收；天气交付包保持冻结，防休眠继续。

- 默认简洁中文，已授权范围内自主实现、排错和必要验证。
- 用户要求连续推进，不要每步都要求用户验证或确认。主机可执行的构建、原版数值对照、回读和内存检查由代理完成；仅在必须使用尚不可访问的 Switch 实机、缺少必要资料或授权时提出具体需求，仍继续不依赖该条件的工作。
- 用户要求 Mac 在完整移植结束前不要休眠；防休眠进程和启动时间记录于 `local/port-awake.json`。阶段完成不解除。完整移植完成或用户明确要求恢复时，核对 PID、命令与启动时间，仅终止该记录对应的 caffeinate 进程；不要修改或覆盖原有电源设置。
- 目标为《尾行3》的 Switch 原生移植，Switch 图形必须使用指定 Mesa 的 Vulkan NVK。
- 当前完成资源、标题与办公室点光照静态预览。不能把可编译、主机 Vulkan 测试或参考游戏的验证当作本游戏可玩/实机验证。
- `尾行3 [汉化]/` 保持只读；分析输出放 `local/`，参考仓库放 `reference/`，生成物放 `build/`、`build-switch/`、`交付/`。
- 原始素材、EXE、存档、补丁、参考仓库副本和交付数据包不进入源码 Git。不自动推送。
- 未实现的游戏流程必须明确显示失败/未实现，不能伪造成功或以演示替代游戏逻辑。
- 全部原生地址只适用于 reports/porting.md 中固定 SHA-256 的中文 EXE。
- 检查入口 `test-host.sh`；只有新改动、失败或未解决疑点才扩大或重复检查。
- 先遵循 `docs/architecture.md` 的模块边界，再按 `docs/roadmap.md` 一步一步实现；不能跳过当前阶段的验收。
- 共用 CMake 目标定义主机与 Switch 构建；版本和依赖从 `config/dependencies.lock.json` 读取。
- `core/resource/model/world/game/save` 不得直接依赖 libnx 或 Vulkan；原格式解析与 GPU 上传分开。
- 尚未实现的目录只定义边界与验收要求，不加入返回成功的假后端；新场景必须经显式注册才能创建。
- 0.3.1 完成 M1.4 的点光照接入，见 `reports/lighting.md`。Switch 默认办公室，保留标题切换；使用基础姿态、检查相机、原版 1 环境光/6 点光的逐顶点漫反射与自发光。FOG 数据关闭，启用的雾/高光/其他灯光类型明确不支持；原版相机、动画和玩法尚未实现。下一步核对原版相机/时间绑定、完整视觉对照和实机验证，M1 整体尚未验收，不能直接跳入 M2。
- `reports/model-cpu.md`、`reports/materials.md` 及相应 JSON 是过去阶段的快照；不得把旧摘要当作当前产物。新的原版对照固定 EXE 摘要，排序 oracle 只覆盖分支与已给定距离键的排序，不等于完整原版帧画面对照。
- `reports/static-scene.md` 和相应 JSON 保留 0.3.0 无光照阶段；当前灯光提交 oracle 验证原版提交参数，GPU 公式检查不等于 Windows 原版截图对照。`reports/camera-inventory.json` 只证明相机资产存在，不能据文件名猜办公室镜头。

- 0.3.2 历史投影基础见 `reports/camera.md` / `camera-verification.json`：原版主循环 `0x4662dd..0x466306` 最终镜头参数为 FOV 1、H/W .75、near .5、far 126384，覆盖早期设备初始化的 1..100000，不能混用。办公室按 4:3 居中，使用经过原版求逆函数对照的视图矩阵，位置仍是检查相机。
- 已验证 `(0,8)` 对应办公室资源；mode 2 初始化选择 `cam00_02.xan / Cam_AUTO`，但这不证明当前流程实际镜头。相机推进分支给 `0x4026fe` 传入 `0.5 * 秒数`，ANIM 键单位/采样、角色锚点与流程状态仍未知。下一步先解决这些依赖，不将导出器相机或基础姿态当成原版游戏镜头。`reports/lighting*` 与 `camera-inventory.json` 保留为此前阶段快照。

- 0.3.3 历史状态见 `reports/camera-animation.md`：12 个 OBJM 相机、19 条完整 SRT 轨道、3,966 键采样已实现；原版循环仅在严格越过末键后截断为整数取余。新增 X 切换的裸轨道检查场景，明确未绑定角色/游戏镜头。`camera.md` 与相应 JSON 保留为 0.3.2 快照。
- 控制器 `0x4bdc12` 在设置平滑后的相机位置之前调用朝向函数；512 次边界对照捕获了顺序、头节点/角色输入和平滑（障碍修正系数 4）。原 ANIM 采样的 14,222 次姿态数值对照包含原资产与合成旋转。片段调度、角色头节点、实际流程仍待恢复，不能把裸轨道的 30 tick/秒诊断计时当作完整游戏时间状态。

- 0.3.4 历史状态见 `reports/camera-clips.md` / `camera-clips-verification.json`：XAN 15 文件的片段配置、一次/循环/链式调度和 SRT 过渡已实现，检查场景改为 `cam00_02.xan` 槽 0 的 151..166 / 400 时间轴 tick。必须区分时间轴和源 tick；速度为 `(end-.1f-start)/duration`，不能恢复磁盘旧速率或计时。
- 原调度器显式选择后首步吞时长、微小过渡第二步仍提交终态；链式选择当次仍提交旧片段源姿态。结束/循环标志保持到选片段。位置过渡使用四次 float lerp 的 smoothstep，不是单次线性混合。
- 朝向/平滑已提供纯 CPU 接口，4096 次真实原数学/帧设置对照通过。尚未接入角色/碰撞/流程，不可给场景填入猜测头位置。下一步追踪角色头节点与动画根坐标、活动流程镜头；M1 未验收。`camera-animation.md` 与 JSON 保留 0.3.3 历史快照。

- 0.3.5 当前角色 CPU 姿态依赖见 `reports/actor-pose.md` / `actor-pose-verification.json`：六个演员的稀疏通道、非单位四元数、头节点查找、无轨道根放置已验证；未实现动态角色绘制或完整游戏相机。`model/playback` 原子提交时间轴与世界姿态，相机诊断场景已使用该接口。
- XAN 0x401b0a 的重复请求必须保留状态（比较 requested，不是 active），不同请求写10 tick过渡；h00槽24允许 chain_after=0。原加载器保留 authored 播放标量；只有显式 authored 构造校验并使用，普通 fresh 构造仍忽略。旧0.3.4禁止旧计时的说明只适用于 fresh 路径。
- 下一项核对实际帧阶段和活动流程：原采样写局部矩阵，原绘制遍历发布世界矩阵。头节点与Cam_AUTO缓存可能是上一绘制帧，不能未经验证立即用新世界姿态驱动镜头；资源加载中间状态3也不能直接决定最终镜头：后续4ebfd0会按进入前主流程覆盖（8/0x38→0，0x48→3，否则area>=8→3，其余→2）。M1未验收，保持连续推进，防休眠不解除。

- 0.3.5 之后已新增 `world/follow_camera`（M1镜头依赖），step/publish 分阶段及720步完整原函数回放通过；当前源码较0.3.5已归档NRO更新。后续线索与CKP散文件路线见 `local/actor-binding-next.md`，不要重复过时调查。

- 后续CPU入口绑定见 `reports/entry-binding.md`；当前源码比已交付0.3.5更新，未重新打包。entry_assets已真实加载45组演员/路线/镜头，但尚未接应用主循环，不能宣布可玩。资源pack routes为Data散文件、每文件限制20480。两种玩家镜头交接/分派已实现，phase1输入控制仍明确失败。下一步恢复NPC动作/移动、玩家输入、碰撞和原主循环时序，不新增假玩法。

- 后续NPC/玩家动画依赖见 `reports/movement-animation.md`：route_motion/npc_motion/npc_point/npc_route/npc_action与player_animation已对照；尚未接完整玩法。playback缓存普通采样time，初值0，同值保持上次locals，blend不清缓存。入口actor_hidden已纠正为actor_fade_out（+331）；直接hidden在+328。
- 下一步恢复4ae783/500ff5等近身/交互判定及真实actor更新阶段。500ff5在area>=8通常返回1（即使未命中），因此办公室AI会提前返回，不能无条件执行npc_action尾部。0x48加载时当前路线点flag改1、+834起点读取的初始flag等可变初始化尚待接入。新报告是快照，SD包仍0.3.5，防休眠继续。

- 后续完整NPC AI与静态碰撞依赖见`reports/ai-collision.md`：已对照4fce2b、三种contact、visibility、ground、ATR静态构造和4b3e6f。完整AI原未初始化choice定义0并单独记录，不声称对所有原栈残值等价。全部29份ATR通过；多子网格原名为父名_序号@模型名，静态碰撞只加世界平移。仍未接应用场景或动态角色，SD保持0.3.5。下一步NPC缓存头/朝向/游标启动及真正actor阶段组合；动态道具追加和玩家墙面滑动尚未实现，防休眠不解除。

- NPC空间阶段与入口实例连接见`reports/npc-spatial.md`：0x4fc8f9..4fcdb6整体对照4176步，真实入口实例360步；局部矩阵独立于世界缓存，place不消耗动画。组1/2朝向的附加节点是hara01/hara02，不是头的直接父节点。游标0启动在头读取之后、地面之前。尚未恢复后续4fd796动作事件及4fc36d完整动画/材质/淡出；新入口接口接CPU实例，不代表应用游戏循环。入口测试共享办公室碰撞为显式夹具，不证明背景分派。SD仍0.3.5，防休眠继续。

- NPC可见性/隐藏调度/独立材质淡出见`reports/npc-presentation.md`：hidden根暂停4026fe时间/SRT，但仍接收401b0a请求及执行fade。零dt不能模拟暂停。42273b更新隐藏节点自身world后跳过子节点。game淡出只出规则，scene提交model材质，不越层。45组入口对照通过，尚无附属物/口型/眨眼完整实例、GPU动态绘制或应用循环；SD仍0.3.5。下一步音量包络/变形依赖和4fd796动作事件，防休眠继续。

- 音量包络/口型/眨眼CPU控制见`reports/face-controller.md`：PCM220样本绝对值/110，包络失败返回0但保留共享状态；表情切换及控制/设置37500次、42820条MORP提交对照误差0。尚未连接实际MORP顶点、脸部外部资源或音频后端，不能声称动态角色完成。下一项MORP数据与变形、动作事件/实际帧；SD仍0.3.5，防休眠继续。

- MORP数据解码见`reports/morph-data.md`：84模型/272轨道/5544键/2467312顶点与原加载代码输出一致；UV1..3残留位须保留，不能因其NaN拒绝。尚未做4316be/432642采样、子集、重绑定、蒙皮或动态绘制。下一步顶点采样及实际脸部绑定，SD不变、防休眠保持。

- MORP采样/过渡见`reports/morph-pose.md`：1696样本/519072顶点原指令对照误差0；精确to仅混合位置不混合法线、重复索引重复提交、blend不改plain_time。CPU目标支持共享绑定，实际FAM/VIX脸部资源映射与ENVL/GPU尚未接入。下一项实际脸部绑定，SD不变、防休眠保持。

- 实际FAM/VIX脸部装配见`reports/face-assets.md`：75份FAM、20套现存演员模型/74绑定/902名称、820步1371696份顶点原版对照通过；entry_assets要求额外挂载faces散文件。控制/RNG/共享顶点原子提交，包含原初始化warm-up。补齐原MORP夹具mesh+84并扩大非选区blend覆盖；未接眼纹理、ENVL、GPU/应用循环，SD仍0.3.5。下一项ENVL与动态角色，防休眠保持。

- ENVL/CPU蒙皮见`reports/skinning.md`：80模型1645网格/1723886影响解码及原变形对照，2077实际样本+800权重边界样本误差0。beta<=.001覆盖首项，累计>=.999补余量；世界坐标输出，绘制须单位矩阵；法线乘骨骼3x3不归一化。尚未接GPU/应用，下一项动态缓冲及角色组合。_55模型实际在bk3_14，之前面部阶段仅查bk3_01；SD不变，防休眠保持。

- 动态角色绘制见`reports/actor-render.md`：Vulkan映射顶点/观察者更新在活动帧之外等待上一提交；ENVL世界坐标配单位矩阵，MORP/刚性部分用缓存节点world，材质实例与隐藏子树进入队列。角色附件存在常量W不等于1，保持齐次裁剪并按原始world*view的XYZ计算灯光；不擅改资产矩阵。新增点光高光和三角形条带/扇面转换；主机`--scene actor`/Switch Y是明确的角色资产检查场景，仍无完整玩法。SD仍0.3.5，下一项眼纹理和NPC完整表现阶段/实际游戏帧；防休眠保持。

- 眼资源/纹理见`reports/eye-assets.md`：FAM眼FRAM与纹理目标分离，槽0原纹理、槽1第5行，空槽保持当前选择，不改gaze variant。20套/5120次原版对照与45组GPU切换/恢复通过；entry拥有、render借用。眼球朝向4a0823尚未实现，需保留local与缓存world/parentworld独立时序；后续继续完整NPC/游戏帧，SD不变、防休眠保持。

- 眼球朝向/增量locals见`reports/eye-pose.md`：4a0823/4f3bb9已对照3300次、角色组合360步995955矩阵；parent-world为每节点上次遍历缓存，程序改动保持到该节点真正ANIM提交。共享原混合精度逆矩阵/非归一四元数。GPU显式gaze探针通过，应用未虚构每帧gaze。组合夹具显式将XAN保存动作编号设0；部分服装如h02_60保存inactive3，当前authored构造仍提前拒绝，下一项按原加载→选片顺序处理此限制和NPC附属物/完整阶段。SD不变、防休眠保持。

- 加载后选片见`reports/actor-start.md`：恢复h02_60 inactive3/h03_61 inactive40，保留旧source为blend_from；重复请求无操作仍严格校验。26原XAN/17712状态/17280采样误差0，眼组合夹具已去掉编号归零、全原始XAN通过。下一项+4附属对象已确认kage_01.xan网格阴影（设置0/1），无ANIM合法实例；设置2为独立投射阴影。SD不变、防休眠保持。

- NPC网格阴影见`reports/npc-shadow.md`：kage_01无ANIM但XAN全速推进，Y+.1、独立隐藏、无主体淡出；360步/21879矩阵误差0，9次GPU/内存回读通过。entry显式可选拥有，尚未组合完整NPC帧；下一项4fc36d组合及动作事件，设置2投射阴影另需恢复。SD不变、防休眠保持。

- NPC表现组合见`reports/npc-frame.md`：4fc36d统一入口接可见性/主体半速/face/阴影全速/fade，hidden仍更新face和fade。360帧原调用者+独立原组件VM对照通过；音量/时钟显式输入，未假装实现音频。actor诊断使用统一入口。后续子步骤失败终止帧，不宣称整体事务。下一项4fd796脚步tick/地面/空间音量；SD不变、防休眠保持。

- 脚步依赖见`reports/footsteps.md`：原4fd796/4afe00、地面比较及50d2a0已对照47385帧/24102事件，latch按源tick共享且不随换片重置，group3首触发退出。entry准备CPU事件，不假装播放。下一项PCM音频/游标/平台输出（2239WAV均PCM16含22000/22090采样率）；SD不变、防休眠保持。

- PCM/播放队列/NPC音频见`reports/pcm-audio.md`：2239原WAV独立解码，队列保留已提交声音版本，口型按消费游标；libnx audout 4x10ms已链接NRO。CPU离线1346880采样与1400游标逐值一致，5入口800帧和真实语音GPU口型通过。主机应用无声，Switch R为资源试听；接口模拟/交叉构建不等于实机声音。主线程队列长帧可能耗尽，暂停/seek/BGM分派/视频和完整流程仍待实现。继续玩家输入/墙面滑动与场景/主流程装配；SD仍0.3.5，防休眠保持。


- 玩家普通空间阶段见`reports/player-spatial.md`：旧朝向移动、两遍墙面/地面、缓存头屏幕投影和真实entry根放置已接通；29场景1392组及组合928组、45入口1440帧/ASAN通过。初始退化边未定义投影采用显式跳过策略，不能宣称任意原栈残值等价；4ae586包含归一化。交互4c20ee/脚本移动4c0d9e、玩家相机和实际主流程仍缺，下一项继续恢复；M1未整体验收，SD不变、防休眠保持。

- 玩家交互/脚本空间组合见`reports/player-interaction.md`：五触发45000调用、状态18000组、脚本14000组、128槽timing331776快照及29ATR组合2088组通过；94退化几何组单列。45入口7200交互帧/ASAN，35host/26Python/29ASAN及NVK构建通过。4c009b已接entry，菜单热键/玩家相机/完整表现/动态道具实例仍缺；下一项4b8a89/4bfea9/4c155d，SD不变、防休眠保持。

玩家镜头见`reports/player-view.md`：九控制器18000组、真实轨道1440帧误差0，45入口7200控制/镜头帧及405路由夹具普通/ASAN通过；36host/26Python/30ASAN及NVK构建通过。aim退化几何显式拒绝；完整phase0→phase1仍需衔接follow写入+43c。继续玩家4bfea9/4c155d表现事件与主流程，SD不变、防休眠保持。

玩家完整4bfea9表现/4c155d事件见`reports/player-presentation.md`：18000事件帧、864原版组合帧误差0，45入口7200控制/镜头/显示/PCM帧普通/ASAN通过；37host/26Python/31ASAN/NVK构建通过。未匹配action原方向字节未初始化，显式0政策单列。noise需与NPC stimulus共享。下一项动态碰撞4b26b8追加/4b2f41清理、背景与完整主流程，SD不变、防休眠保持。

动态道具碰撞见`reports/dynamic-collision.md`：4b6fa3 12000组、十真实模型120批原4b26b8/4b2f41对照误差0；静态前缀+原子动态后缀/清理、kind字段及3245网格ASAN通过。37host/26Python/31ASAN/NVK构建通过。下一项511940生成、5121ce移动、512102显示与背景/主流程。帧初碰撞必须读旧发布world，SD不变、防休眠保持。

背景CPU/音频见`reports/background-lifecycle.md`：45配置/94音槽、18000原帧/162460命令、73XAN/43800采样、66真实装配/559368矩阵误差0；90入口真实PCM普通/ASAN一致，38host/27Python/32ASAN及NVK构建通过。自动链4025b9与显式选择不同：空目标不切换，非零静态零时长产生-Inf，原x87负速率unordered钳制终点；不可误判原门初态无效。尚缺雪花相机父节点、雨精灵、背景GPU/光照分派、中央主帧；SD仍0.3.5，防休眠保持。

背景GPU/雾见`reports/background-render.md`：90入口720帧145496实例/6启雾入口普通及ASAN一致；12000原雾状态回放、320透视GPU采样通过，39host/28Python/32ASAN/NVK构建通过。42d295先设mode3，后续vertex模式不清table模式；range=0/disabled记录也不主动清旧设备状态。环境适配器已支持原高光/FOG；探针仍用全点光和检查镜头，非游戏分派。下一项4a435a/4a4438/4a4701选灯与绘制阶段；雪花父节点/雨/中央主帧仍缺，SD不变、防休眠保持。

原版选灯与绘制分派见`reports/lighting-pass.md`：16000分派/262552命令、58配置381灯4234灯光快照误差0，90背景720帧普通/ASAN一致；40host/28Python/33ASAN/NVK构建通过。帧内每个灯光组使用独立描述符，更新保留相机/雾。中央根槽绑定/全局绘制队列与设置2投影仍缺；下一项雪景相机父子关系（42403c是local setter，world=local*缓存parent），雨精灵/中央主帧继续推进，SD不变、防休眠保持。

雪景相机父节点见`reports/snow-camera.md`：14入口840帧550494份矩阵误差0，66背景回归误差0，72雪景GPU帧普通/ASAN一致。root_local用旧parent缓存立即更新rootworld，publish_under才刷新整层级；advance必须显式保留playback当前rootlocal，NULL会恢复资产根。42407a逆父矩阵允许常量W舍入；奇异父缓存world setter拒绝。真实雪花重生缩放3e-7，已修正相对行列式检查和shader法线尺度。40host/28Python/33ASAN/NVK通过；继续组4雨幕16精灵与中央主帧，SD不变、防休眠保持。

雨幕见`reports/rain.md`：原4fac10在绘制阶段每精灵两次共享RNG，先Y%trunc(1280*scale)，后X%trunc(960*scale)-100；scale=窗口宽/1280，尺寸256x512。81配置4000帧214164顶点误差0；18配置216GPU帧27648像素普通/ASAN最大1/255。41host/28Python/34ASAN及Switch库构建通过。新雨组件尚未接应用，NRO因死代码移除与雪景阶段相同。继续中央主帧/模型全局队列，不按阶段结束，防休眠保持。

全局提交队列见`reports/draw-queues.md`：完整42aa47回放2400次/38292项/14378距离误差0；普通键不随负载交换、纯透明绕过排序、常量W与sqrt精度保留。actor批次收集原始遍历并统一排序，背景/门共同提交，静态检查同步。GPU纹理创建ID是显式可移植策略，不等价Windows指针或原纹理别名。43host/28Python/35CPUASAN及NVK通过，NRO b249f171…；后续继续全局节点挂接/51c736和中心帧，SD不变、防休眠保持。

主绘制51c736/4d9733见`reports/draw-dispatch.md`：18k根选择(17131原回放/869缺gallery拒绝)覆盖256流程；16k组合帧/40类事件条件、回调顺序及描述符编辑一致。722454+24是DirectSound GetStatus，4e01e4压低/恢复722334音量，不是视频绘制。必需事件服务缺失即失败；尚未恢复4d9898实际场景/4e1473AVI/4e01e4音量或主应用绑定。44host/28Python/36ASAN及NVK通过，NRO仍b249f171…(新组件未链接到诊断场景)。继续全局节点挂接/中心帧。

全局frame_tree/单节点缓存发布/actor节点批次见`reports/frame-tree.md`：5000原拓扑操作/60050绘制发布/31710提交一致；两真实模型160帧128800矩阵与原D3DX误差0。attach强制刷新忽略hidden且用global缓存world；draw剪hidden且用global local；嵌套请求的选中latch会延续到外层后续兄弟，不能简化子树。16GPU夹具9216像素普通/ASAN、45演员ASAN、45host/28Python/37CPUASAN与后续节点接口单元通过。NRO a488da27…；下一项场景装配器绑定真实节点/加载顺序，再中心帧。拓扑模块不会自动写actor缓存，必须消费walk；SD不变、防休眠保持。

真实姿态全局装配见`reports/actor-forest.md`：借用actor、原子挂接/刷新/相机预遍历与160帧128400矩阵原对照误差0；换父节点后的GPU旧祖先隐藏误判已修正。90背景720帧普通/ASan一致，46host/28Python/38ASan及NVK构建通过。继续相机轨道纳入注册表、全场景加载顺序与中心帧；SD不变、防休眠保持。

相机轨道/入口装配见`reports/track-forest.md`：follow_camera使用真实ActorPose共享缓存；960原版组合帧24000矩阵误差0，45入口296实例720帧普通/ASan一致；46host/28Python/38ASan/NVK通过。已确认4bf1a0会重新加载玩家+阴影。entry_forest按原顺序绑定已实现根，后续4ec9f0的bk3_16/item模型尚缺，继续恢复该加载/隐藏/动画与交互后接中心帧。SD不变、防休眠保持。

物品加载/显示/拾取见`reports/items.md`：45入口31实例，保留yaw/hidden、全速动画，4f5c6a有序拾取已接scene并纳入entry_forest；原2880加载/4050显示/12000拾取帧通过，31实例124帧GPU普通/ASan一致。47host/28Python/39ASan/NVK通过。拾取在显示之后，下一显示阶段才实际隐藏。真实se101音效与51876a提示服务仍待接入，继续后再中心帧；SD不变、防休眠保持。

拾取真实PCM和原字节提示见`reports/item-feedback.md`：4693原查找、1200组合帧/2385拾取通过；45入口31拾取/119364离线PCM样本普通与ASan逐值一致。48host/28Python/40ASan/NVK通过。FTT/提示绘制与中央帧仍缺；继续恢复字体显示，SD不变、防休眠保持。

FTT/原字节对排版见`reports/font.md`：65536字符码、7476位图/6767168像素、1875排版/48375字形原对照一致；345CPU遮罩、50Vulkan帧512万像素普通/ASan差0。49host/28Python/41ASan/NVK通过。当前只完整字形遮罩；原滚动/显示缓存/多遍混合和提示状态仍缺，继续实现，SD不变、防休眠保持。

文字滚动/双缓存/原多遍混合见`reports/text-display.md`：16000时序帧、6000绘制/89656顶点、780组合帧38338560像素原对照一致；150Vulkan帧576万采样普通/ASan差<=1/255，含4:3子视口。49host/28Python/41ASan/NVK通过。新确认beeccc是面板计时器armed而非phase，下一项修正命名并接提示面板/图标/计时；local/item-ui-next.md有线索。SD不变、防休眠保持。

提示面板/五图标/共享5000ms计时见`reports/item-notice.md`：18960原段帧、1200精灵帧7200顶点一致；75拾取/310GPU帧1027万样本普通ASan差<=1，49host/28Python/41ASan/NVK通过。notice_phase已更名notice_timer_armed，与pickup共用；消息/图标保留生命周期明确。当前NRO687b520d…；未接完整应用，SD不变、防休眠保持。继续51a682中央帧及4f4306/4f3c80/4f3d34。

发现结算/完整4f3d34区域规则与真实双通道声音见`reports/area-boundary.md`：24000发现、17280双边界、4500完整区域帧/35911道具/532声音原对照一致；10实际cue82320PCM样本普通/ASan同hash。NPC区域音+568独立于脚步+448和语音+984，玩家仍用+458。50host/28Python/42ASan/NVK通过，NRO仍687b520d…，未接中央帧。下一项4f4306；直接DS Play/Stop需要暂停/保位续播而非重播，继续实现。

完整4f4306与音频保位transport见`reports/prop-interaction.md`：6000原帧/35806道具/2379声音命令、45实际配置744帧1428480PCM采样普通ASan同hash。game借用既有motion/sound/player/NPC共享字节，scene用真实root缓存；暂停保留分数相位与排队声音。50host/28Python/42ASan/NVK通过，NRO4fd8c111…，SD未变/应用仍诊断。下一项中央51a682/4ec78d与phase0→1，防休眠保持。

开场4c1313与follow距离+43c见 reports/player-idle.md：29真实ATR928原帧（828定义良好/100原退化边单列）、720原镜头步、45实际entry1080帧97200保持矩阵/45距离衔接普通ASan通过。纯actor request不推进；idle保留vertical/velocity和阴影；+43c为NPC到旧Cam_AUTO的XZ距离。50host/28Python/42ASan/NVK通过，NRO568e69be…。中央帧仍需AI/路点音效；phase0/2到1含51a190对话/UI不能只写phase。持续推进、防休眠保持。

NPC AI/等待/路点音效见 reports/npc-event-audio.md：5140原选音/48初始化、16929原路线/4176空间快照一致；3840000真实PCM普通ASan同hash71e7068bef82f978。NPC+568与区域共享，AI7299c0/729ae0独立，globalBEF050=se002单独生命周期；route保存pre-ground位置。50host/28Python/42ASan/NVK通过，NRO869f79e5…。下一项实际51a682/4ec78d，应用仍诊断/SD不变，防休眠保持。

中央51a682/4ec78d装配见 reports/game-frame.md：2400原调度/17674事件、45入口1080真实组件帧及PCM普通ASan同hashb28d69fe1e966e4a；碰撞后缀失败也清理，phase锁定/NPCmode在追加之后。动作short-range为player_actions[8] (71b544)。50host/28Python/42ASan/NVK通过，NRO仍869f79e5…。跟随障碍目前必需注入，probe540次为显式miss；接下来恢复4bef54/4bed86/4b61e5并嵌入预平滑后以保留缩短+43c。初始化/51a190/应用尚未完成，防休眠保持。

跟随障碍见 reports/follow-obstacle.md：2896原mesh查询(2596定义良好/300原NaN单列)、77gates、完整原follow+真实collision720步/46缩距最大误差0；45入口1080中央帧普通ASan通过。真实障碍已直接接中央帧，去掉外部miss回调；+43c和player_view.probe同步。50host/28Python/42ASan/NVK通过，NRO35acd685…。下一项实际入口状态初始化/保留动作槽，4bf18e只写单槽，NPC未写的+1c/+24不能假定2/5，当前组合probe明确夹具。防休眠保持。

演员入口状态见 reports/actor-entry.md：11520玩家/2768NPC原指令初始化最大误差0，166真实资源入口+45组1080中央帧普通ASan hash201816ecf775411f。NPC walk[1]/run[1]及timer deadline/armed、run_remaining、hidden保留；玩家vertical=basehead+10，NPC=basehead+正常spawnY，flow48玩家位置覆盖。route先取start flag再改current1。新增npc_entry_route区分+834/+838；玩家outcome别名同步，AI库存3/4直绑。50host/28Python/42ASan/NVK通过，NRO323ebf44…。继续外层4e82b8(48先把area映射4/5/5/6/5)、新游戏/存档全局来源，初始化API不等于完整入口。防休眠保持。

外层入口目的地/进程初态见 reports/session-entry.md：11520resolve+1024mode2 reset原对照0，27游戏CRT无hook执行。boot_state仅进程首次，lean10/seed传time低32；重入保留。resolve flow48先映射area4/5/5/6/5再绑定cursor/start。45实际返回+166演员入口+45profile1080中央帧普通ASan同hash201816ecf775411f；50host/28Python/42ASan/NVK通过，NRO1a4ed1b6…。下一项51765d/517c8e共享对话状态与51a190，外层完整资源生命周期/应用仍待接；防休眠保持。

对话元数据解析见 reports/dialogue.md：resource/dialogue完整517c8e和51765d扫描，26真实文件296open/3960next原对照通过；i00_10无标签区域表单列拒绝。保留pending媒体/current标签、next每次仅清F；引号body不推进cursor、#才推进；端点比较等号。4096变异/截断、50host/28Python/42ASan/NVK通过，NRO1a4ed1b6…(未引用parser会裁剪)。下一项4ec528/6c0/777开场phase与真实文本/点击音，完整51a190未完。防休眠保持。

开场对话/交接见 reports/opening.md：7200原51a190 branches0/2/3截至51a3bc对照、620close、6000原idle11提示；45入口2927自然交接帧普通ASan同PCM c39ec8b4443233cd，82FTT上传。共享面板/字体/点击se004，保持旧phase分支与关闭保留metadata；Vulkan120开场+310物品帧普通ASan像素误差1/255。50host/28Python/42ASan/NVK通过，NRO仍1a4ed1b6…(应用未引用裁剪)。下一项公共HUD/黑屏转场及4cbca4/4cb902，完整生命周期/可玩应用未接；防休眠保持。

公共HUD/转场请求见 reports/common-hud.md：13440原51a190 tail/51c47e对照，1030release/703pause、6目标模式；se100/se007是DirectSound Play保留位置，捕获音乐音量。真实PCM1631调用/2689920样本普通ASan同f2b5506e13dc864d，Vulkan12黑幕+120开场+310物品普通ASan误差1/255。50host/28Python/42ASan/NVK通过，NRO仍1a4ed1b6…。mode50加载、暂停UI/实际场景生命周期尚未完成；下一项操作HUD48/30元素及4cbca4/4cb902，防休眠保持。

操作HUD见reports/player-hud.md：48/30实际资源及保留槽、显隐/闪烁/状态条/图标/数字/顺序已恢复。71b828是玩家npc_in_view，不是stance。60原构造/9000帧/1255440顶点精确一致；Vulkan320帧8050206通道普通ASan误差1/255，45入口2927组合帧/360phase1通过。51host28Python43ASan/NVK通过。prop零矩阵仅在available!=1时保留未使用depth并标记unprojectable；available1失败必须终止。49d0eb仅保留必须的绘制服务边界，下一项截图、暂停、拍照及热键/flow50，防休眠保持。

实际截图见reports/screenshot.md：帧内GPU读回+颜色/深度LOAD恢复、精确HUD边界、红key水印、规范BMP、原照片名与原子文件输出。16原编码695288像素/40原名字一致，192GPU截图6193152字节、5pause+5photo7464960字节普通ASan精确一致。53host28Python44ASan/NVK通过，NRO879e6464…。scene输出由app接save，不越层；存档未实现。继续热键/暂停/flow50及可玩应用，SD不变，防休眠保持。

热键见reports/player-hotkeys.md：4c0126在交互/移动之前，仅旧script_phase0；三组独立pause/camera/photo，原声音0/5/7与截图配置已接。menu/count/table归game_frame，HUD直读；51917c帧前group锁存album_group；actor重载清menu保留照片。10240原组/11280配置完全一致；真实PCM2688000普通ASan同aac424628fb8ffaf，GPU7464960RGB字节一致；45入口2927帧/135请求同c8c7c5b8866f45ab。声音槽64，54host28Python45ASan/NVK通过，NRO368033a0…。继续暂停/flow50及可玩装配，防休眠保持。

flow50见reports/flow-loading.md：完整51c4bf借同一common curtain/blocked，mode1背景→黑幕退出→load→黑幕恢复，special previous38再等确认；mode0/2加载切换、3只切换。原16769帧4471load/82confirm/25626draw一致，119原几何名字一致。真实5素材160GPU帧22913460通道普通ASan误差1；55host28Python46ASan/NVK通过，NRO368033a0…(应用裁剪)。load仍必需实际场景绑定，下一项20精灵暂停界面5158a0/5162d9/5171b5。local/pause-layout.json及pause-state-refs.log已恢复布局；防休眠保持。


首关应用及持久流程见reports/play-session.md：实际标题→开场→phase1→照片→暂停恢复→返回标题→flow50重入已接。play_session拥有一次boot的GameFrameState/进度与同一common/flow/cursor/pause；game_preview入口借用状态。每tick必须draw/end/after_present，零tick重绘无副作用；黑幕副本预备alpha、提交后真实tail加载当帧截图，逻辑release先停音、GPU下一步帧外回收。暂停51a77c仅继续音乐/持续环境音量更新，背景计时/RNG/动画保持（后续原4f720c对照纠正）。1700帧应用普通/ASan两照片通过；最终play-flow普通/ASan开场994/暂停136/标题141/加载125/重入995通过；8针对性CTest/架构/NVK通过，NRO1ad6a2b4…，无LeakSanitizer/实机证据。下一项保存读取/失败与任务结算等缺失目标，不能以首关移植完成；每道具头部保留传感字段仍需实绑。原素材只读、防休眠保持。旧SD包保持，新开发包独立目录。

退出已按原466448的flow58终止主循环；4e7671对58无资源。play-flow新增141帧退出普通/ASan通过，打包素材实际应用1621帧正常退出/两BMP通过。最终开发NRO107d86f9…，独立包`交付/开发验证版-20260927/switch/biko3`共126文件约1.795GB，历史SD不覆盖。磁盘剩余空间有限，不盲目复制全素材；当前default打包10PP+CKP/ATR/FAM/FTT。目标仍active，Mac防休眠PID60608保持。

失败HUD51afcb与5镜头见reports/failure-transition.md：HUD12000帧438消息4596release原对照，camera12000步4061完成最大误差0；2单元普通/ASan与架构/NVK通过。signed delta<=.1不可改abs；先旧world aim后装新平滑XYZ，overhead不改FOV、其他.2。HUD panel先advance后request，prompt相反；outcome6首次10401+group*10000消息，二次黑幕，40/2release→68mode2→字段reset。新组件未接应用，NRO裁剪保持107d86f9。下一项4eb0ee资源与51b244组合（local/outcome-flow-next.asm、next-51b244.asm、failure-camera-*.asm），再flow68重试菜单/存档任务。未执行全套旧oracle，不重跑旧record_*脚本。磁盘约2.3GiB可用，避免新增全资源副本。

失败与重试应用已接通，见reports/failure-session.md及JSON：原4eb0ee/4bf7db30表+2400状态、51b24412000帧56100回调、4eb9d4/51bbe925构造10300帧原对照通过。实际5组6结局2700帧/5184000PCM普通ASan同hashdc31b61e287de260；自然输入失败→重试→游戏→失败→标题普通/ASan均opening994 game589/726 outcome3/2 menu187/63 retry-opening995，GPU回读已查看。7CTest/5ASan单元/1700帧应用回归/架构/NVK通过，NRO71c453d1…未解析0。新游戏入口仍开发首关，完整游戏未完成。下一项flow20区域完成保存询问及4e94e4关卡交接；原资源只读、防休眠PID60608保持。

区域保存询问/角色交接组件见reports/area-handover.md：51a7a2 25构造10300帧原对照，4e94e4 40区域2560非零状态最大误差0，40真实CKP/XAN替换与资源失败原子性普通ASan通过。角色/动画/头缓存/全矩阵/face/eye/shadow/库存保持，4befc0不立即place模型根。已纠正4cc320为空的旧结论：它恢复HUD reserve709c64=1，app在锁存phase0/2帧尾执行。1700帧应用/架构/NVK通过，新NRO77df6790…。flow20和flow28尚未注册，下一项真正世界资源/保存页面装配，不能当已可通关。防休眠PID60608保持。

区域flow20应用及数据存档见reports/area-session.md、checkpoint-storage.md：40真实菜单/世界/GPU交接6400帧普通ASan通过（显式区域完成夹具，非自然通关），实际失败重试/1700应用回归通过。原存档1000写/1000读/10000记录逐字节一致；版本化50槽/CRC/原子文件替换普通ASan通过。应用flow28仍待注册，下一项507540/5095d5保存页及恢复。快照NRO2b053b2f…未解析0；完整移植未结束、防休眠PID60608保持。

当前语言目标更新：先完成日文版，使用未修改的日文PP和Type_S/G.FTT。flow28存读档已接应用，详见reports/save-session.md；不要恢复Dest.ftt依赖或打包汉化覆盖。原指令对照仍基于既有固定中文EXE，日文原始EXE带保护壳（SHA ad353d250ebb04cc38fc78894111427a20780162e371914b00234c44a7b72606），不得声称已获得日文原EXE逐指令/全帧验证。存档文字明确做Shift-JIS本地化并单独对照。完整移植未完成、防休眠保持。


日文原标题组件见`reports/title-menu.md`：20原构造/10352帧、缩放11760步70560顶点完全一致；真实素材360GPU帧29476092通道、691200PCM样本普通ASan一致，最大像素差1/255。六目标38/28/30/18/60/58。仅选中图片推进，键盘warp不改本帧旧指针，半尺寸构造先truncate。零售缺te_10试玩资源明确失败。5相关CTest/单元ASan/架构/NVK通过；应用仍开发标题、NRO与存读档阶段相同。继续38角色选择504335/504802、4bac5b/4bb612相机与真实资源，再8/应用；防休眠保持。

菜单镜头数学见`reports/menu-camera.md`：6000环绕/6000轨道原步误差0，4相关CTest/ASan/架构/NVK通过。4bb612先新位置后aim；0角环绕变360，三角函数精度不可合并。真实菜单XAN/forest尚待装配，应用/NRO不变；继续38选择页及8，防休眠保持。

选择镜头资源见`reports/menu-camera-assets.md`：第一组cam01_50为文本X，新增显式姿态解析（网格仍不支持）；三文本1303键/5427矩阵、真实双轨1200原帧/83763矩阵误差0。五组1800普通ASan帧及4096变异通过，3CTest/架构/NVK通过。mode1全局refresh忽略hidden，mode0与辅助轨道保持；应用/NRO不变。继续504335/504802选择页、演员音频与flow8，防休眠保持。

选择UI见`reports/selection-ui.md`：20构造/7376原帧/129748绘制/29153事件误差0；日文41图、5语音、bg002，850GPU帧普通ASan最大2/255，1632000PCM同hash33fae992235d7070。5CTest/架构/ASan/NVK通过。绘制在控制前；标签同时裁宽和UV；音乐800dt。资源替换块仍必需服务，应用未接/NRO与SD不变。继续实际选择演员/舞台/脸部/灯光/音频和51ac5d，再任务8，防休眠保持。

Switch实机反馈跟踪减速/CPU波动/断续爆音（最早静态办公室正常），修复见`reports/switch-performance.md`：game改每次呈现一次真实elapsed，250ms极长帧保护有日志；audout独立2ms线程+锁/原子clip引用/游标保留与PCM缓存刷新；刚性网格缓存不变顶点、保留材质/MORP恢复。4200计时、60/30/20/15Hz完整应用、12×150ms主线程停顿+300并发声音命令普通ASan/TSan、1346880PCM误差0，540演员/720背景GPU普通ASan、67CTest/架构/NVK通过。上传4.64→1.11MB/帧，应用回读完全相同。修复NRO dfe24cbe…与增量包`交付/开发验证版-20260928-时序音频修复.zip`；尚无新版实机帧率/声音证据，历史包保留。新增PERF每2秒日志用于后续定位。线程必须先stop/join再释放mixer/设备；cursor PCM到该voice下次cursor查询才失效。继续选择演员/舞台/flow8，`local/switch-performance-next.md`记录未完成的selection_actor验证；防休眠保持。

原版游戏时钟/FPS恢复见`reports/game-clock-fps.md`：用户确认时序音频包仍慢动作；原4adc16的1ms重装计时器隔帧取样，中间帧复用旧733700，稳定频率游戏dt累计约墙钟2倍。上一版直接用单帧elapsed会约半速。新core/game_clock完整采样保持/.22上限，wall计时器/音频独立；17,820原帧误差0、15/30/60应用、play-flow497开场/498重入及save-flow5362帧普通ASan通过。恢复原4adc96的FPS锁存，默认显示（便携位图替代GDI）；3CTest/2架构/NVK通过。新包`交付/开发验证版-20260928-原版速率与FPS.zip`，NRO f530d486…；原计数器边界可能60Hz显示61，PERF另记真实平均FPS，game/wall原版稳定值约2。未获新版实机效果；旧修复包保留。按明确wall调用play_session_step_at，不能用game_seconds累加菜单/face时钟。选择演员CPU对照已通过尚待归档，舞台spotlight依赖未实施；继续主线，防休眠保持。

选择演员CPU装配见`reports/selection-actor.md`：3840变体/10240规则、10演员960原帧/1644708矩阵/1608192顶点最大误差5.56e-17，1800普通ASan帧通过。主体.4*game_seconds（原733700），focus名字按变体表；61视频仅必需边界未实现。舞台m60_00无灯，正常60演员含两个spot(type2)，现有适配器仍拒绝；继续spotlight/舞台/选择完整场景和flow8。FPS修复包已冻结，后续修改不要重跑其打包脚本；防休眠保持。

选择聚光灯见`reports/spot-lighting.md`：4352原提交/2560绘制快照/2048超1强度分派逐位一致，方向为(-world[2],-world[6],world[10])并经原522922归一化。合计8点/聚光，h01_61实际强度1.08合法；66GPU案例和320雾采样普通ASan差≤2/255，90背景720帧/10演员1800帧回归通过。67CTest/29Python/30Hz应用普通ASan/NVK通过，源码NRO c1200ed0…；交付FPS包f530d486…保持。继续实际选择舞台/相机/演员forest、重载和51ac5d；special=1的4bb9de镜头及alternate视频需单独恢复，二者标志独立；flow8仍缺，防休眠保持。

选择世界/3D组合见`reports/selection-world.md`：五镜头/50替换/600原51ac5d帧、2145630矩阵误差0；2400CPU帧普通ASan通过。25正常人物组合600GPU帧/20旧快照跨替换普通ASan同hash b1fe7bfb62bf7df7。初始body恒group0、辅助镜头才用retained_group；重载保留舞台/轨道时间，旧body须等旧GPU owner退役后释放。3CTest/架构/NVK通过，NRO未变。world的movie/voice目前必需外部真实服务，GPU明确拒绝61视频缺失，special1镜头另缺；未接应用。下一项实际语音/音乐/UI生命周期、视频和flow8；交付FPS包保持、防休眠继续。

选择音频见`reports/selection-audio.md`：bg002/五语音/1200世界帧、960独立PCM包络逐值一致，2115840样本普通ASan同hash e410f89b1c6058ae。绑定停止clip不能用play→pause两命令，避免音频线程夹缝发声。NPC与选择共用scene/voice_audio，抽取前后WAV及游标日志完全一致，240步语音GPU回归通过。音频CTest/架构/NVK通过，源码NRO8ae19588…，FPS交付包不变。继续UI/世界/音频生命周期、61视频与flow8；防休眠保持。

正常选择页组合见`reports/selection-session.md`：394帧真实UI/世界/音频、8次换人、2次重绘及开始8/返回1请求普通ASan同RGBA b8090df06476d674 / PCM6dac0477cade1bf2。draw→end→after_present后才允许下一步；旧GPU/演员下一步回收。保留共享UI/镜头/包络/RNG/解锁表；原variant抽到61仍明确失败。架构/NVK通过、应用入口未接、交付FPS包不变。下一项poi.avi动态纹理/flow8，再原标题入口；防休眠保持。

AVI/选择61见`reports/avi-texture.md`：两MSV1文件180帧/600seek与FFmpeg逐像素一致，原521ed2计时18000帧/9113复制对照0。保留原+44/底向上DIB行序；紧凑RGB555、初始黑/尾2像素黑是明确便携政策，不是Windows VFW布局证明。Vulkan复用暂存/帧前上传，480视频帧普通ASan同hash；25套61世界600帧/20旧快照、394会话229视频帧普通ASan通过，正常60结果不变。200固定诊断镜头帧3组观察到视频像素变化，其余2组可见性未证明，不夸大原镜头覆盖。4CTest/架构/NVK，源码NRO854a416f…；FPS包f530冻结。下一项flow8实际资源/人物/对话/菜单与目的地，先读4ef6a0/4f11f2/4f1503/51b9b3；它不能未经恢复直接当作进入游戏2。special1另缺，防休眠保持。

2026-09-28 用户性能优先更新：默认Vulkan compute ENVL蒙皮，详见`reports/gpu-skin-performance.md`/JSON；80实际模型+100权重夹具6071208顶点，普通ASan最大位置误差9.54e-7。普通队列缓存原交换置换（透明仍动态），去掉单模型重复排序、forest二次乘法、背景矩阵临时复制和空动态碰撞分配；原排序2400/7200缓存、矩阵128400、动态碰撞120批均通过。主机上传1.12→.18MiB；CPU侧耗时估计2.11→1.90ms，不能宣称Switch60FPS。`reports/cpu-static-review.md`记录余下动画临时内存、碰撞扫描、Vulkan状态/单帧缓冲、灯光与音频锁候选。BK_CPU_SKINNING=1仅显式诊断；shader float32不与CPU double逐位相同。

同时修正贴墙镜头查询：app误把已收缩camera.world+12当射线终点，必须用前一阶段player_view.probe（原71af38+524=71b45c）；统一`bk_player_view_collision_query`。原控制器产生probe再由原墙体读取，4800帧/9600调用误差0；15/30/53/60Hz抖动夹具稳态峰峰0。见`reports/camera-probe-fix.md`。保持既有controller/probe/cache时序，不额外阻尼或放宽碰撞。新独立包`交付/开发验证版-20260928-GPU蒙皮与CPU镜头修复.zip`，NRO fca3726e…；旧FPS包f530/9405哈希冻结不变。普通ASan完整应用/照片暂停重入存档通过；实机效果尚未验证。主线flow8/dialogue_actor仍未完（local/dialogue-actor-next.md），不得宣布完整移植完成；PID60608防休眠继续。

对话人物CPU/GPU子集见reports/dialogue-actor.md：17457规则/1200固定镜头、五组1440原帧2314920矩阵2397024脸顶点最大4.45e-16；任意槽完成计数323200次。27文本#E实际0/1/4/5/6/7/8/9覆盖，负值继续拒绝。1800CPU/600GPU帧普通ASan一致，GPU灯光/包络仍显式夹具；4CTest/架构/NVK通过，应用尚未引用/NRO仍fca3726e。接下来4f1503换图和4f0e44真实对话/媒体/UI，然后原标题入口，交付包冻结、防休眠保持。

存档/撞车反馈优先排查见reports/save-car-crash.md：碰撞后动作使所有道具零步推进，静止0/1点路线被严格检查拒绝，完整应用已复现；现静止保持位置/朝向，原负前驱跨表读取作为明确安全策略，不称逐值等价。45配置14880实例帧/1860停步、两次car outcome1→失败重试主机普通ASan通过。存档/截图新增no-overwrite FS备份替换/回滚/恢复，100写/4故障/2中断/10截图及实际应用同槽4存+5组重入5599帧普通ASan通过。用户强调旧版可存，不能把兼容缺陷直接说成此次近期回归根因；暂无Switch日志/实机证明。GPU蒙皮保持，所有者分配统计/保存与转场边界/清理前错误日志已补；Switch保留一份nvk.previous.log。新包另建，fca与f530历史包冻结。主线dialogue_backdrop尚未完成，见local/dialogue-actor-next.md；防休眠PID60608保持。

后续用户实机反馈：现在Switch存档不闪退了。保存修正获用户确认，记录reports/save-car-switch-feedback.json；不再把“等待日志确认保存是否恢复”当阻塞。无撞车实机反馈，不推断碰撞已验收。继续主线flow8，冻结交付包不改。

对话背景与独立黑幕见`reports/dialogue-backdrop.md`：完整原4f1503共16685帧/1372替换/12连续切换、原初始及替换几何960绘制5760顶点误差0；真实6图987GPU帧18455724RGB采样普通ASan最大1/255，FNV0788e8fa0cb56d81、资源回基线。保留外部wanted、两种宽度缩放及背景request→advance/黑幕advance→request顺序。新增单元/架构/NVK通过，应用未接flow8，NRO仍3052908a…、交付包冻结。下一项4f175d/4f18cd真实语音/音乐和4f0e44推进，再完整flow8；防休眠保持。

剧情语音/音乐见`reports/dialogue-audio.md`：原6000语音/18504音乐帧/2000初始化/23063命令误差0；五组真实人物1800帧、原轨迹1930命令驱动参考混音器，3171840PCM与1540口型包络普通ASan逐值一致（FNV47a141ec115c4f31）。保留先旧wanted gain再pending、-6000换曲、空曲保位停止、语音release/load停止/restart0。缺媒体明确失败且保留pending；单元/架构/NVK通过。应用未接flow8，NRO/交付3052908a保持；下一项4f0e44翻页/文字/结束分派，防休眠继续。

用户追加反馈Switch撞车也正常；存档/撞车均获用户实机确认，见reports/save-car-switch-feedback.json。用户询问原标题/选人未接入：已说明组件已完成但交付仍开发标题。接下来优先把原标题→选人38→剧情8→游戏2完整接通并另包交付；不得把未实现flow8直接跳过。

剧情文字/结束分派见reports/dialogue-ui.md：原12000帧/2713翻页/1813转场/308解锁请求、26实际脚本2273页/6946目标、640原几何绘制误差0；465GPU帧47069625采样普通ASan最大1/255。共享pulse原6000步与既有应用普通ASan完整flow回归通过；新NRO747ced95…，冻结交付仍3052908a…。解锁仍必需服务非假成功，完整flow8尚未接。接着入口脚本/初态/灯光与场景装配，再按用户优先原标题→38→8→2另包；防休眠保持。

原标题/选人与真实flow8已接入默认应用，见reports/original-front-end.md及dialogue-entry.md/dialogue-session.md。原入口18316选择/120真实首段、40入口3716页；world1440帧2314920矩阵2397024脸顶点最大4.44e-16。五组会话3097帧813页、RGBA a05fbf855005ebde/PCM955098fa1676e6a9普通ASan一致。原标题→五人切换→首人剧情→游戏→照片暂停→原标题普通ASan与纯手柄回放通过；默认实时启动160帧、标题读档取消800帧普通ASan通过。修复挂接早于根放置的缓存顺序、bf00f8计时/UI别名、flow50加载38前提及初屏时钟基准。存档5599帧/同槽4写与撞车两次失败重试再次正常回归；用户旧修正包存档/撞车均确认。NRO aca3370fa1bd6b7989065519749bc8f0a61f3facc6bccd4c0f4b394522635f56；默认game显示原版标题，旧开发入口须BK_DEVELOPMENT_ENTRY=1。新增bk3_18必需资源。设置30/鉴赏18与60/结局解锁持久化仍缺并明确失败，不能宣布全移植完成或新包实机通过。防休眠60608继续。

已另包交付：/Users/rujian/Documents/Codex/WX3/交付/开发验证版-20260928-原标题选人与剧情接入.zip；ZIP SHA256 202b2aca84534aa3a3b26d6908af336e8bfb00767800d2f6b1dfa5730fc14982，NRO aca3370fa1bd6b7989065519749bc8f0a61f3facc6bccd4c0f4b394522635f56，逐文件ZIP回读通过。新增bk3_18，其他素材/存档不写；新脚本local/package-original-front-end.py已冻结，禁止对同名产物重打包。旧save-car/GPU压缩包目前不在工作区，仅保留历史摘要；未重建或修改。后续继续未实现设置/鉴赏/结局持久化等，不以本包作为完整移植验收。

独立鉴赏进度存储见reports/unlock-storage.md：原50c6bc/50c8a1/50ca48，6000写6001读6000工作副本/CRC逐值一致；72字节CRC封装，保存输出save/unlocks.bku，保留8字节非布尔值且按行替换非OR。普通ASan/模拟no-overwrite FS的100写、72损坏、4失败、2中断通过；正常启动有效/损坏表检查与已完成进度夹具全菜单/游戏回放普通ASan一致。缺文件默认0，不复制原IO失败产生伪解锁的bug；未自动导入原Yellow文件。源码NRO a0a8a1221163505aaa81585693a24fe814005e57d5690424b53c96c232c23bb0，原标题交付包aca3370f冻结。重要：完整结局事件/工作表/自动提交尚未绑定，front_end.unlock仍明确失败。下项恢复flow16入口4eb875→4cc582、真正帧分派及原标志生产，再提交；51b9b3是鉴赏选择，不是flow8/16总帧。设置/鉴赏UI与完整移植仍未验收，防休眠保持。


结局帧内核见reports/ending-frame.md：原4d74c9..4d7a95共20000帧/60625调用/8638标志变化/721结束请求误差0，21必需子服务、原phase锁存/尾部实时状态、两次时钟/严格>30000ms/AL完成判断已恢复。普通ASan/架构/NVK通过；实际flow16更新51b56a→4d7436、HUD51b4c9，51b9b3是鉴赏。工作列0,1,3,4,2,5，不含后两项；尚未接子控制器/资源/应用，NROa0a8a122…及原标题交付包保持。继续4cc582/实际镜头和角色控制，禁止空回调假成功，防休眠继续。


结局四种镜头数学见reports/ending-camera.md：4bb0a4/4df411/4e1711/4e1d16共20000原帧误差0，选人共享orbit抽取后12000原帧及五组1800真实资产帧普通ASan通过。自动镜头推进后仍读旧world，无预发布；手动保留+4a4瞄准前矩阵、world瞄准后，不能合并。纯手柄完整原标题流程普通ASan原时钟回读仍6cb3f86d…；架构/NVK通过，源码NROdfa9beb0…，交付aca3370f冻结。结局资源/应用及4e0ecb/4bc444未接；继续4cc582真实装配，防休眠不解除。

结局双轨实际装配见reports/ending-camera-assets.md：50原加载、4000组合帧/291550矩阵、1000整数根旋转误差0；50配置6000普通ASan帧同hash953c44bfb7fbb56a。固定cam00_00/Cam_AUTO与cam00_03/locator1，挂接→选0→仅根旋转，自动更新读旧world；目标仍显式夹具，完整结局/应用未接。单元/架构/NVK通过，源码NROdfa9beb0…及原标题交付aca3370f冻结。下一项4e0ecb/4bc444已确认是预设矩阵过渡，不推进XAN；随后真实角色/整体入口，防休眠保持。

结局预设过渡见reports/ending-preset.md：4ae043矩阵4000组、4e0ecb/4bc444各6000原帧误差0；真实六镜头6000帧435550矩阵误差0，9000普通ASan帧同hash81628feeabe22993。两进度独立、仅完成更新+4a4、条件FOV/.6每秒收窄、无XAN推进；原索引mod3已确认。2CTest/2架构/NVK通过，源码NRO5843a8c0…，原标题交付aca3370f冻结。完整结局角色/公共操作/子阶段与应用仍缺，继续真实入口，防休眠保持。

结局完整公共控制见reports/ending-control.md：4d7ac4..4d901b的13独立区域、预设/目标/开关/暂停/悬停，24000原帧131741调用及快照逐字节一致；单元ASan/架构/NVK通过，NRO5843a8c0…及原标题交付aca3370f保持。495d92仍必需实际服务，尚未接真实UI/应用；下一依赖为XAN实例动态chain/next与source写入，不能修改共享set或以advance0替代。防休眠保持。

XAN实例动态编辑见reports/clip-edits.md：15XAN12000原帧/1541自动链误差0，五演员900帧2113146矩阵最大1.11e-16；15演员3600普通ASan帧同c6831d7109897d54。原子私有chain/next/source及配置request_mode，不推进/发布。原标题回归同6cb3f86d…，NVK8a2735dd…通过；495d92仍未实现。当前优先用户报告实际游戏天气漏接，原标题交付包冻结、防休眠保持。

实际游戏雨雪与零缩放修正见reports/weather-integration.md：组4雨/组0雪area0..6已接应用、失败、重载及bk3_20打包。72实际雨帧8294400RGB普通ASan差<=1/255；40区域重载/325天气快照、雨雪原标题流程、同槽4写/五组读档、两次撞车失败重试通过。雪花精确零缩放仅对无像素退化三角形跳过，其他奇异变换仍拒绝。NVK 2ead894e…通过，新天气无实机验收。原标题交付冻结，下一恢复495d92/结局子服务，完整移植未完成、防休眠保持。

结局辅助动作495d92见reports/ending-auxiliary.md：20000原帧/37042服务、五FAM主角1500链帧与600完整SRT帧1410066矩阵（最大1.11e-16）通过。实际4946b4/49490a/4e0956/4ad2bf共1446调用3612命令，2548800PCM普通ASan同1092d25cb4d5e81d；五角色3600组合帧/30音频配置同矩阵efb11f3ce00589b1、PCM69f7f8469b311b11。六声音槽/eye1/私有clip编辑已接，expression只保存待face阶段消费；完整flow16未接。70故障前缀/3CTest/2架构/ASan/NVK通过；NRO仍天气2ead894e…，两个交付ZIP哈希保持，防休眠60608继续。下一项实际结局入口与其余子服务，不能用当前probe的action4/SE夹具代替初始化。

结局入口/角色依赖见reports/ending-entry-assets.md：4cc7e6..4ccabd共8000原分派/5008服务快照，保留加载后读group、正常40KB块FF/计数0和最终加载后phase8。BOM4a5870七原文件/五合成/4542变异截断通过。4f2dae专用FAM构造4眼/3口（普通仍3/3），50配置2050原步3483688顶点误差0；6000普通ASan帧10196160顶点同e35a98113a34273f，旧20配置原/ASan回归通过。4CTest普通ASan/2架构/NVK通过，源码NRO574d92a2…，两个交付包冻结。实际BOM节点/顶点回调、完整loader/flow16仍缺；不以metadata搜索或空服务替代入口，防休眠保持。


BOM节点对齐/顶点映射见reports/bom-deformation.md：原2430位置朝向调用、8941映射/3140回调/788620顶点误差0；实际ActorPose600对齐/7400矩阵误差0；7夹具840帧4300440顶点普通ASan同33e849f6a68b780c。422c49只按reference设位置，不重挂父节点，纠正旧next笔记。kahansin缺mata.vix为显式空选择夹具，不声称完整资源加载。新2单元+actor-forest/eye-pose回归、4ASan/2架构/NVK通过，源码NRO b4fd1fef…；原标题/天气ZIP冻结。完整4a4dc3生命周期/控制与GPU蒙皮后BOM回调、flow16未接，继续推进，防休眠保持。


固定调用动画/MORP组见reports/fixed-animation.md：26160原混合时钟/258330计数、7真实辅助模型840帧21024矩阵1691118顶点误差0；1680普通ASan帧2779920顶点同d62fdd5043b45bfd。40168c比较active；4021a1不吞首步/不处理blend，按秒入口即使dt0也清调用计数。MORP组相同时间跳过包括mask变化，源模型可释放；hidden不推进/发布。4普通4ASan/15演员编辑回归/77976按秒原样本/1700应用帧/2架构/NVK通过，源码NRO f4722317…；两个交付ZIP冻结。仅clock/ANIM/MORP，不含MATA/其他效果；完整BOM/GPU后回调/flow16仍缺。下一项MATA，防休眠保持。


MATA材质动画见reports/material-animation.md：确认+158/4300e0，+160/4270e0属于LIGA。78真实/60合成15360原采样9843314已定义字段误差0，63946未初始化W策略单列；7辅助模型840帧ANIM/MATA/MORP组合误差0。78模型9360普通ASan帧同63d646cba531d7c8；7材质诊断平面420GPU帧80640RGB普通ASan差0，同d51b58261b830ce5，无重复时间上传/分配增长。2普通2ASan/2架构/NVK通过，源码NRO d1df27ad…；两个交付ZIP冻结。完整BOM/GPU后回调/按秒效果与flow16尚缺；按秒MATA读取实际source，不能用blend_from替代。防休眠保持。


BOM实际CPU装配见reports/bom-assets.md：完整4a4dc3节点/刷新/映射/固定ANIM/MATA/MORP/隐藏，7真实配置420原帧539490矩阵810810顶点6241映射21140材质字段误差0。1680普通ASan帧2779920顶点同ac2010275e22b6d2，22预检失败保持；空名字按原深度优先可命中未命名节点，kahansin缺mata.vix原零计数已确认。5普通5ASan/只读GPU计划单元/2架构/NVK通过；应用未引用，新NRO仍d1df27ad…，两ZIP冻结。仅新加载两模型CPU绑定；每帧BOM控制、GPU蒙皮后回调、按秒效果/4a52bc和完整flow16仍缺。防休眠保持。


BOM蒙皮后GPU回调见reports/bom-render.md：通用628276字段与七实际配置504帧2580264顶点，普通ASan最大相对误差2.38419e-7；alpha0/hidden/重复提交/冻结姿态/纯重绘/保留所有权和颜色深度视口通过。6241选中目标均有ENVL影响，回调后下次begin重算，当前完成输出仍可诊断回读；不退回CPU蒙皮或帧内读回。131模型2206936顶点ENVL回归、雨天手柄原标题流程/两照片、5普通5ASan/2架构/NVK通过。源码NRO58b0cdc5…，天气/原标题ZIP冻结。每帧BOM控制、4a52bc、按秒MORP组blend及完整flow16仍缺，防休眠保持。


BOM输入/回弹/跟随见reports/bom-motion.md：31500原指令组误差0（含节点别名），七实际资源420帧1001910矩阵最大2.842e-14；1680普通ASan帧同564a45e84bdbe0d3，504GPU帧2580264顶点最大3.57628e-7。primary reference相对parent控制，逐帧auxchild跟reference，初始binder则跟parent；三个手控/两回弹状态跨模型重载保持，单节点提交不改子缓存/时钟。3单元+foundation普通ASan、雨天完整原标题回归/2架构/NVK通过，源码NROc9339ee8…，交付两ZIP冻结。完整4cf318、另一4a52bc、按秒MORP组blend及flow16仍缺，继续收尾；用户确认雨天实机正常，仅此范围，防休眠60608保持。


BOM按秒ANIM/MATA/MORP组合见reports/bom-seconds.md：原组混合700帧1158300顶点误差0；七实际owner840原帧1996113矩阵2999997顶点最大2.842e-14。按秒输出独立实际source给MATA，ANIM/MORP用blend端点；旧普通入口无额外状态查询。1680普通ASan帧同75837de64f3fb5f4、504GPU帧2580264顶点最大3.57628e-7；5单元普通ASan、雨天原标题完整流程/2架构/NVK通过。源码NRO36290e9f…，冻结ZIP不变。4a52bc三模型绑定、真实4cf318等加载与剩余结局阶段/flow16/设置鉴赏仍缺，防休眠保持，继续收尾。


普通结局生产CPU装配见reports/ending-normal-assets.md：原4cf318十入口12618矩阵/14016MORP顶点/604材质字段/5096映射误差0，10表情预热/RNG一致；30加载1800组件帧普通ASan同185c0a6e9d0433ca，五缺资源失败保留外部状态。FAM来自fambom；动作表variant与相机variant分开，普通相机恒0；主体/辅助/BOM/双轨/固定镜头/组1背景保持顺序。七BOM回归同75837de…，1新单元普通ASan/2架构/NVK通过，源码NRO5743e95a…，冻结ZIP不变。尚缺GPU组合/视频/灯光/共享状态/UI与其他结局loader，flow16未接；用户已确认雨天实机正常，防休眠继续。


普通结局外层背景/共享灯光见reports/ending-background.md：forest追加保持旧ID/拓扑/缓存；4cc582按表加载90/91，组1保留92。十入口原追加后13197矩阵/时钟/目标误差0；20灯光配置9024命令/3456原提交/1280快照一致。30加载1800组件帧5400灯光快照普通ASan同67b0813bb822e972，24缺背景失败与注册/启用容量拒绝通过；3普通3ASan/2架构/原标题雨天手柄回归/NVK通过，源码NROf1ff22ea…，冻结两ZIP不变。雾设备、完整GPU/视频/特殊绘制/UI/flow16仍未接；用户雨天实机反馈仅覆盖天气，防休眠60608继续。


普通结局常规GPU装配见reports/ending-normal-render.md：实际主体/辅助/背景、三批灯光队列、GPU蒙皮后BOM、FAM/眼/MATA/MORP与poi.avi已组合。原600遍历529080缓存矩阵/10578网格节点误差0，证明可见末尾背景先发布后复用几何。十配置320帧22980队列项1707968顶点普通ASan最大3.68802e-7，40纯重绘/40空绘制/320视频变化及十次缺视频清理通过，分配稳定。2架构/NVK通过，NRO仍f1ff22ea…（应用未引用裁剪）；两ZIP冻结。当前仅常规mode1，隐藏背景/特殊4d9898/完整事件UI与flow16仍缺，防休眠60608继续。


结局特殊双镜头调度/局部深度清除见reports/ending-special.md：完整4d9898共12000原帧319701服务/24000绘制/62178显隐/605材质调用与540镜头行150名字精确一致；空名字/重复命中/实时状态和35失败前缀通过。原Clear确认D3D7深度flag2，54包装/2000转发/500视口构造一致。Vulkan120合成帧921600通道/120次LOAD恢复普通ASan精确、分配稳定；2单元/2架构/NVK通过，源码NRO94783412…，冻结两ZIP保持。尚缺生产场景适配和双视角GPU快照，不代表flow16接入；继续收尾，用户已确认雨天实机正常，防休眠60608保持。


结局特殊镜头真实CPU适配见reports/ending-special-scene.md：十配置240原帧/1920快照、4000瞄准、2559264矩阵/2049792材质字段误差0；动态跨模型查找/显隐与独立相机local/world/parent缓存已接。423e72只更新view、不提前发布角色缓存。十配置1200普通ASan帧同b38c6950c3480f7f，30失败检查、3普通3ASan/320常规GPU回归/2架构/NVK通过；源码NRO9c58594b…，两ZIP冻结。仅CPU生产适配；两视角GPU快照、其它loader/子阶段/UI/flow16与设置鉴赏未完。用户已确认雨天实机正常，防休眠60608保持，继续双视角GPU。


普通结局双镜头GPU见reports/ending-special-render.md：两份主体/辅助几何、五批灯光队列共享纹理/AVI和灯光状态，实际局部depth clear/视口恢复，背景不复制。十配置320帧31924队列项3415936顶点普通ASan最大3.68802e-7；320第二遍网格/240BOM目标与顺序单缓冲参考一致，20错误预检、40纯重绘/40空帧通过。80合成帧268800通道误差1/255，8无纹理复制view/4释放顺序和GPU分配基线通过；320常规GPU/原标题雨天手柄回归RGBA6cb3f86d…/2架构/NVK通过，源码NRO8f11bae1…，两ZIP冻结。未接完整flow16/其他loader/UI/子控制器；继续4cc582完整入口，防休眠60608保持。


结局入口声音/4e01e4见reports/ending-sound.md：12k原控制/26218服务与十入口460加载精确一致；48共槽中辅助2..5就是效果0..3，不能独立复制。41有效效果+4原越界名字无资源空槽；音乐独立，压低的是语音0，保留两次读值、719c5c与拒绝越界gain政策。3600帧4861440PCM普通ASan同1df9c66e102ad8d5，40别名替换/560拒绝gain/十退役/缺音乐与五末尾损坏失败通过；旧六槽2548800PCM同1092d25c。3普通3ASan/线程同步getter/2架构/NVK通过，源码NRO771b9109…；冻结两ZIP保持。完整入口复位/UI/子控制器/flow16仍缺，雨天实机已确认，防休眠60608保持。


结局UI布局/动画/GPU见reports/ending-ui.md：62元素24次原构造/720连续帧267840顶点，17.7k独立动画帧106200顶点、6k侧栏/2782按键与13控制区域精确一致。实际62图240GPU帧11135742通道普通ASan误差1/255，同7dbe9e017f9983d0；8纯重绘/10拒绝、分配回收稳定。1普通1ASan/2架构/NVK通过，NRO仍771b9109…因应用未引用裁剪，冻结两ZIP保持。完整4d499b调度/重载与flow16仍缺，下一线索local/ending-ui-next.md；雨天实机已确认，防休眠60608继续。


结局黑幕重载控制见reports/ending-reload.md：原4d9354/4d9575和4d679d..4d6e6e共14k组/579272服务及实时状态一致，1900回调变更；36配置2274失败前缀普通ASan通过。实际48音槽2400帧3248640PCM同a147043ab2164e07，1640原暂停/40语音等待/1720续播/40别名替换；phase0为显式无资源夹具。1普通1ASan/2架构/NVK通过，源码NRO771b9109…与两ZIP保持。仅控制与必需资源服务，完整4d499b/额外精灵/实际各loader/flow16未接；继续local/ending-ui-next.md。雨天实机已确认，防休眠60608保持。


结局分支UI/实际图片见reports/ending-stage-ui.md：240原构造/30600更新183600顶点/1360释放误差0，51实际图片30配置480GPU帧14976444通道普通ASan差<=2/255，同b37ec6d5349fa642。共享cursor8不复制；63..74保留标量，30旧快照跨重载/60重绘/93拒绝与分配回收通过。基本UI原版及240GPU回归同7dbe9e01，2普通2ASan/2架构/NVK通过。NRO771b9109与两ZIP保持。完整4d499b/flow16未接；下一先补未加载handle的50e6ba推进（原先更新再检查绘制，不能直接跳过），再主UI，见local/ending-ui-next.md。防休眠60608保持。


结局UI公共调度见reports/ending-ui-scheduling.md：无图片/模式0共13824原组（11424定义正常、2400空句柄策略单列），实际popup2400帧；6000线段/16000圆测试和工具栏2400帧57855绘制347130顶点误差0。scene/ending_ui_toolbar接4d4a06..4d4ed8，借同一frame/control/aux/open，保留重复推进与回调实时状态。800实际线段快照/480GPU帧14975382通道普通ASan差<=2/255同d7ed665da77df4cb；旧动画/分支/基本GPU回归、3普通3ASan/2架构/NVK通过。NRO771b9109及两ZIP保持。完整4d499b/flow16尚未接，下一4d4ed8分支提示和后续主UI；防休眠60608保持。


结局分支提示/进度条与游标显示见reports/ending-ui-hints.md：四分支3200原帧21106绘制、首次初始化/原指针捕获/工具栏组合800帧26039绘制、游标提示2400帧30240绘制误差0（1444空句柄策略帧单列）。4b757e输出数值float运动量，非整数位模式；phase3增长不额外乘dt。游标先draw后request，无图片游标不推进，popup仍推进；notice独立于pause_flags。两套600GPU帧普通ASan同51c82bf597a581ac/2b1aa0a6cecda642，差<=2/255；2普通2ASan/2架构/NVK通过。NRO771b9109及两ZIP不变。完整UI选择/黑幕/尾部与flow16仍待接；防休眠60608保持。


结局目标命中见reports/ending-ui-pick.md：4da3ca/4da4f7读取相机local(+80)，4a7b26及完整4da76f/42d4b6已恢复。4000角度/16000线段/1612整查询无hook原执行一致，12目标均覆盖，含99零W/76零距离。零W原屏幕值为(-10000,-10000)，slot50半径与float/double中间值保留；缺节点原栈未初始化明确拒绝。1普通1ASan/2架构/NVK通过，NRO771b9109/两ZIP保持。下一实际4db3e4/4797dc和主UI选择，尚未完整flow16；防休眠保持。


结局实际游标选择见reports/ending-ui-select.md：完整4db3e4/4797dc各2000原帧、主选择1585帧，选择→显示1885帧24587绘制147522顶点误差0；回调实时重读/低8位按键/语音slot1门限保留。10原表row14目标-1绕开未赋值路径，未知phase无覆盖分别415/515组明确拒绝；phase3对照用正常入口variant1表，鉴赏生命周期仍待恢复。600GPU帧/2304000真实PCM样本普通ASan同a805b067f0cb87e9/9f73ccf45a412b05，差<=2/255；1普通1ASan/2架构/NVK通过，NRO771b9109/两ZIP不变。下一共享黑幕与UI尾部，完整flow16未接；防休眠保持。


结局共享黑幕/完整UI尾部见reports/ending-ui-tail.md：实际495567/48cb8a、830语音选择/4200尾部帧/600黑幕帧/8268顶点误差0，保留advance/request差异、即时正计时计数、RNG、实时回读。实际385声音名/770原替换确认143原素材空项（含显式夹具组合），有限空槽兼容不吞损坏/意外缺失。五真实演员800GPU帧/3072000PCM普通ASan同f29915bcad45cf7e/aef6eee5465dccb5，286空替换及损坏覆盖通过；既有PCM逐值回归、2普通2ASan/2架构/NVK通过。NRO771b9109/两ZIP不变。下一完整UI组合/重载快照生命周期与flow16，未宣布结局可玩；防休眠60608保持。


结局完整UI帧/跨重载图片见reports/ending-ui-frame.md：4d4979/4d499b共1800原帧78108绘制468648顶点、240加载/80实时回调/60早返误差0；phase7未赋值选择200帧显式-1政策、1135空图片设备策略帧单列。共享common真实字节/身份校验/失败前缀；GPU25组200帧3180120通道普通ASan同a7863893d73ac8bd，25重载/200重绘/1025拒绝/分配回收通过。3普通3ASan/旧62图GPU回归/2架构/NVK通过，NRO771b9109及两ZIP保持。完整flow16/上层照片HUD及其它loader/子控制器仍缺，下一入口真实复位与资源表所有权，防休眠60608保持。


结局入口状态复位与资源表绑定已完成，证据见 reports/ending-state.md；原4cc582..4cc7e6 6000帧/103区/34008000字节误差0，30组真实日文资源装配与50条资源路径普通/ASan一致，2项架构与NVK交叉构建通过。完整flow16、GPU普通结局帧、视频、结局UI/音频和解锁仍在继续，雨天实机反馈已确认，防休眠保持。


普通结算场景已接入统一应用生命周期，见 reports/ending-normal-session.md；两个真实 gallery variant 各64帧、普通/ASan Vulkan及应用8帧通过，普通CTest100/100、ASan91/91、Python29项和NVK交叉构建通过。完整flow16、特殊结算和结算实机仍未验收，雨天实机反馈保持通过，防休眠继续。

结局入口/退出修正见reports/ending-lifecycle.md：按4eb8f6→50ca48在加载后首帧前复制已保存40字节，保留非布尔值/其它组；group校验保留。重载音频回调接全48槽，应用逻辑stop先停51自有槽，最终快照仍可绘制；重复stop/延迟销毁不清下一所有者的槽。普通/ASan十项、两架构/两CTest/NVK通过，NRO27513a2f…；未整包/未推送，防休眠60608保持。重要：frame_invoke里多原阶段仍只接通用姿态推进，必须逐个恢复原控制器，不能把64帧初态探针当完整结局；phase8/9、其它loader/UI重载离开和自然解锁写回仍待完成。

结局phase9确认见reports/ending-confirmation.md：完整4e2223原12k帧/24528服务/724回调变更/787失败前缀精确一致，旧common原24k回归一致。真实故事场景通过输入打开弹窗，普通/ASan各247帧取消键/点否恢复与确认请求保持通过；侧栏命中框每帧更新，CPU import→export、UI后import修正共享请求覆盖；warp只改指针，A/B不虚构Z/33450别名。新增取消se002/槽60，52自有槽停音与12外部槽保留、延迟退役通过。指定NVK NRO7703dfa0…、未整包/未推送/未实机，防休眠保持。确认仅到wanted/action，ui_reload_schedule仍失败；phase8/各角色阶段及完整退出未完成。六个BkEndingLeave入口已确认是状态复位并非GPU析构，接续local/ending-confirm-next.md按原别名恢复，禁止空成功。

结局返回标题/退出见reports/ending-exit.md：六个leave入口补充反汇编成功，按既有别名和新增retained所有者恢复复位；49739a的5546a4写float.5位模式，482f91两小表各10十进制字节，最终40KB工作区不是持久录制表。故事场景借应用同一common及真实schedule，action45→标题1执行六次复位，47→58不复位；ending_first_present保证构造快照先呈现再更新。显式剧情后入口夹具下，普通/ASan实际输入各196弹窗帧+124黑幕帧、436800音频帧通过，52槽停音/外部保留、末帧重绘/标题重入/终止/GPU基线/解锁文件不改通过；非自然完整结局。ASan四单元、四场景及原标题手柄回归、两架构/NVK通过；NRO125ce676…，未整包/推送/实机，防休眠60608保持。新原指令对照和普通单元额外重链接被自动检查以“无法确定请求的安全状态”拦截未执行；普通场景批次已启动但结果读取被拦截，退出状态未确认，不计通过，不绕过拒绝。六种复位原指令验收仍缺；继续各原阶段控制器、phase8、其它loader和自然解锁链，详见local/ending-exit-next.md。


最后界面画幅/光标修复见 reports/ending-viewport-input.md：scene 统一居中 4:3 viewport，UI 初始化/绘制/点击均用内容尺寸，触摸减偏移；core 虚拟光标接左杆/十字键/ZL，实际位移进入 words6/7；修复 Vulkan 投影后重复翻转 Y。普通/ASan 各12组3368帧，UI显示像素差0，13按钮/确认取消/边界/GPU基线通过；完整三维像素对照11/12，1080p gallery0边缘差异仍单列未通过。前端/游戏回归、架构/NVK通过，NRO c31dc6f0…；仅交付增量NRO，不修改旧SD/天气包、不推送。无新增实机或整体结局验收，防休眠60608保持。
