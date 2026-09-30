# 特殊剧情真实音频、视频纹理与绘制（2026-09-30）

本批将 `scene/special_world` 接到实际 PCM 音频、GPU 演员、灯光和视频纹理所有者，并修正特殊剧情灯光分类。普通／ASan+UBSan配对通过，指定 Mesa NVK 交叉构建通过。新所有者尚未接生产 flow48 UI／应用生命周期；鉴赏 action8 继续明确拒绝，不能据此宣称特殊剧情可玩或完整移植完成。

固定中文 EXE SHA-256 为 `a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`，实际资源为日文 Data。终态记录为 `build/validation/special-media-9_jg1458/verification.json`，归档见 `reports/special-media-verification.json`。

## 声音所有权和原版行为

`scene/special_audio` 借用 mixer、资源服务和四个原有 loop 字节，拥有四个效果 clip 与一个独立音乐 clip。效果位置仍由原 `BkSpecialEventBindings` 提供，包络仍借用共享708878／7C，未复制进程状态。

| group | 循环音乐（初始-6000） | 初始效果0 | 初始效果状态 |
| --- | --- | --- | --- |
| 0 | bg036.wav | se120.wav | 循环播放 |
| 1 | bg041.wav | se122.wav | 已加载、停止 |
| 2 | bg037.wav | 空 | 保留原loop字节 |
| 3 | bg040.wav | se143.wav | 循环播放 |
| 4 | bg038.wav | se121.wav | 循环播放 |

表格来自4E2C11..4E2D1E、4E3E50..4E4187实际加载片段。动态效果按原4E5901选择se400／410／420／430／440。50D858的flags0为停止且非循环，1为立即循环，2为停止但保留循环元数据。46435E先回到源位置0再播放；直接DirectSound Play适配为保持播放位置的resume，两者不可合并成统一重播。group1进入阶段2后启动效果，30秒后改变loop规则且不倒带。

50DB23淡入淡出按float到int截断、至少1步和实时音乐设置限幅；50D2A0空间音量／声像继续使用原数学，并按类别读取音乐／语音／效果实时主音量。口型读取实际已消费PCM位置，效果不存在时返回0但保留共享包络。每个原时间查询点单独走服务。

加载初始音乐和效果全部成功后才发播放命令；缺音乐、缺初始效果、损坏初始效果均拒绝且保留已有声音和外部loop字节。逻辑stop仅清槽一次；析构只释放引用，旧owner晚释放不能清掉新owner声音。动态LOAD释放旧clip、由队列保留尚未消费的epoch是明确的移植资源管理策略，不复制原覆盖指针造成的资源泄漏。

原指令音频夹具执行完整50D4FA、50D858、50DB23、46435E、50D2A0及上述加载片段；仅资源／DirectSound叶边界供给实际WAV和第二个独立混音器。原50DE05的缓冲大小查询提供明确常量夹具，未验证Windows缓冲大小元数据或DSP位一致性。两路可移植混音器的命令、游标、PCM和包络相同不等于实机声音验收。

## 灯光分类修正

原4E418E对4A435A传入NULL；此前special_world沿用普通场景BK3_L分类错误。本批增加带显式key的灯光构造，五个主体全部登记为group2，继续原mode1／group4的mode10和slot4主体绘制。

固定EXE导入表53F0F8是KERNEL32 `lstrcmpA`，旧运行时与oracle叶服务都误用了忽略大小写比较。现改为真实ASCII名称的大小写敏感等值判断，并区分NULL和空字符串。[Microsoft文档](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-lstrcmpa)确认其大小写敏感；[Wine的Windows兼容测试](https://github.com/wine-mirror/wine/blob/master/dlls/kernel32/tests/locale.c#L1766)记录非NULL空字符串与NULL比较非零。NULL叶边界据这些证据实现，没有执行原Windows DLL，也不声称覆盖任意locale／DBCS排序。

重新执行58个原背景和5个特殊剧情主体的原4A435A／4A4438及绘制计划对照，63配置406灯光／4599绘制快照一致。旧忽略大小写oracle不能作为这次修正的证据；此次更新了叶服务并重新对照16000个计划、4000个ambient初始化、4000个名称分类，命令264657条、误差0。

## GPU与视频

`scene/special_render` 借用真实CPU world，拥有一个bk3_14演员、一个提交队列及独立灯光快照；不另造背景模型。prepare按原森林子树发布和灯光命令顺序捕获模型、MORP、GPU蒙皮、Back绘制开关与当前相机；group4的尾部灯光切换不覆盖此前绘制快照。相机校验森林anchor1，投影保留4:3、near .5、far126384。

group1／4加载真实bk3_18／poi.avi，并替换唯一D_moza.bmp纹理表面。仅原movie服务显式推进，prepare与draw不推进视频时钟。沿用已有AVI RGB555／源+44行拷贝策略与未首次解码前黑色表面的明确可移植行为；这不是Windows VFW布局证明。旧GPU快照在新world／render构造后仍可重绘，GPU对象必须先于借用的CPU资源销毁。

## 终态验证

`tests/check_special_camera.py --suite media` 已接 `test-host.sh` 的EXE＋Data分支。本轮按范围运行该套件，没有运行整份历史入口。普通与ASan+UBSan四组结果完全相同，每种构建：

- 音频原指令对照900帧／900包络、4328命令、39加载、5退役、3构造失败；1213440个PCM标量，SHA-256 `e027a72053bc72a42598a44763919777c5a89af0f922c81d0d166670b57f3fa9`。
- 上述63真实灯光配置和16000计划／4000初始化／4000分类对照。
- 真实资源组合1800CPU帧，其中1450帧进入阶段2；179次准备绘制、159次纯重绘，4次旧资源快照退役、2次缺视频拒绝且GPU分配回到拒绝前基线。实际控制发出9个动态音效加载、2次直接Play、720次视频更新；两组实际材质合计3828个变化像素。
- 128358个实际GPU蒙皮顶点与CPU结果对照，最大相对误差 `3.28356722e-7`（既定上限5e-5）；图像摘要 `ae428451e400d429`，组合离线PCM摘要 `f8ac913837b99ded`／6912000标量。
- 6项普通／6项ASan CTest（灯光、蒙皮、包络、AVI解码、AVI时钟、声音队列），另29项Python检查通过。ASan关闭leak检测，不能表述为完整泄漏证明。

GPU组合中的clock、无按键、fog、加载phase／sequence边界是显式夹具，未实现剩余UI。视频可见性检查使用实际视频网格世界顶点边界构造检查相机；真实自动镜头可能不包含该表面。首轮随意放置检查相机未看到纹理变化而失败，日志 `local/special-media-smoke.log` 保留；修正检查相机后通过，生产绘制没有因此改动，也未放宽像素断言。首次新增CMake目标未重新配置而不能构建，随后配置成功；不算实现通过前的构建记录。

指定Mesa26.2.2 NVK提交 `5dba7886c56460ff47c3038e323d07b9547d6212`、libvulkan.a摘要 `d77231522a7338bbb016cbaa9bd364a41d5484e1becac8569dfcc1fbea87c256`；ELF未解析0。NRO为16179256字节，SHA-256 `328bd51b60a4b08929413bade087ebcc1ef9bcc71bd7ae389d1199cbdd731c56`。新音频／render符号存在于Switch静态库，生产ELF尚未引用；NRO变化包含共享灯光修正，不代表flow48入口已接通。

继续4E4472 UI、51B617／完整4E29B0／4E42E4和应用生命周期，再自然故事记录与持久解锁／返回链。完整3D像素此前9/12及Switch实机缺口保持。防休眠64008保持，只本地提交、不推送、不整包；完整访问never，无需再请求终端授权。
