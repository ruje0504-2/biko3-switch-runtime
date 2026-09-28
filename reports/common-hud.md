# 公共HUD与转场请求

scene/common_hud实现51a190尾部51a3bc..51a671：先推进并捕获黑幕透明度，再处理1000毫秒计时门限/显示请求，然后按outcome、菜单请求、response的原优先级处理；黑幕stage3时才调度，菜单action2单独创建暂停UI后写flow4。保留重复outcome、response2..4未清零、同帧菜单被response覆盖、timer有符号比较及未知原字节等行为。global beeb7f是操作锁定，和开场notice_visible不同；主flow与camera.phase也保持独立。

game/flow_transition实现51c47e请求：mode不等于2时先调用实际资源释放服务，再写previous/target/current50/mode；mode2保留场景。所有需要的音频、调度和暂停加载服务都是必需回调，未提供不返回成功。初始化仅映射4e6dee黑幕字段，4ebfd0重入仅清timer.armed/gate，保留其余状态。此层不虚构flow50加载器或尚未实现的暂停界面。

scene/outcome_audio拥有原4e82b8的se100.wav(B537D8)与se007.wav(B53558)，不同voice，使用加载时BE9A0C音乐音量。它们调用DirectSound Play(0,0,0)，重复播放请求保持位置，自然结束后的再次Play才从0开始。与46435e每次重置的点击声分开。curtain_render持有实际bk3_00/ma_01.tga（16×16全黑不透明），按原1280×960布局放大，顶点alpha先截到8位；使用common_hud在请求之前捕获的透明度，在phase HUD后绘制。

验证：13440组原51a190尾部、原4c2609/4f75dd/51c47e执行对照，1030次release、703次pause，所有6种目标/模式组合一致；原指令只在时间、DirectSound Play、资源释放和暂停装配处设服务边界。原开场7200组与提示6000组回归通过。两种实际PCM共1631次Play请求、27次起播/自然重播，独立连续重采样参考校验2689920样本，2675586非零，普通/ASan同hash f2b5506e13dc864d。

Vulkan普通/ASan各通过12黑幕帧、120开场帧、310物品帧，14791680像素独立混合检查，最大误差1/255。50host/28Python/42CPUASan/NVK构建通过。NRO 1a4ed1b6a82210733a2b227072a7ef64cc92d3f08cd795b408da19b197f3cb63，undefined0；未接应用的组件会被裁剪，不以相同NRO声明已可玩。

接下来恢复4c9bf0/4cbca4/4cb902实际操作HUD、键位与公共状态绑定，然后flow50/暂停等资源生命周期和应用注册。本报告只证明公共尾部与具体音频/绘制组件，不证明真实世界已跨场景切换或可玩/实机验证。SD归档0.3.5、防休眠保持。
