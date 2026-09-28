# 角色选择双轨道与文本镜头姿态

`scene/menu_camera_assets`加载cam00_00及五组cam01..05_50；两轨道选槽0，仅辅助轨道在加载时推进一次。保留当前camera world、matrix与focus，初始化平滑XYZ=(0,20,0)、环绕yaw/pitch=0、radius=22、height=18、FOV=1。资源所有权与全局forest分离，销毁forest后才释放轨道。

自动模式完整执行主轨道全速推进→423be2全局刷新→新Cam_AUTO/focus读取→位置/朝向→相机安装；刷新忽略hidden，动画仍依原hidden暂停。手动模式不推进轨道；辅助轨道在此帧阶段保持。全局锚点必须为单位矩阵，错误绑定和不支持状态明确失败。推进后下游失败视为致命帧错误，不宣称全部事务。

第一组辅助cam01_50.x是DirectX文本。新增显式`model/x_pose`相机姿态接口，读取层级、局部矩阵和0/1/2 SRT键，保留源文本并构造供已有ANIM采样器使用的私有数据。原44795f将WXYZ转为XYZ(-W)，时间0保留原初始化种子。网格/材质仅保留为有界不透明文本，不生成空白可绘制模型，也不改变通用model_decode对文本几何的不支持结果。类型定义参考[Microsoft AnimationKey](https://learn.microsoft.com/en-us/windows/win32/direct3d9/animationkey)和[Frame](https://learn.microsoft.com/en-us/windows/win32/direct3d9/frame)，具体转换采用原游戏指令证据。

固定中文分析EXE a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e：三份日文文本资源、1303键、540采样/5427矩阵，与原447667/44b671/采样及D3DX比较最大误差0。输入服务边界为堆、对象注册查找和sprintf；关键数据转换/插值运行原指令。主轨道与原4bb612/4bac5b/423be2在同一VM组合回放1200帧/83763矩阵误差0；辅助轨道由独立VM校验原选片和加载时推进。

真实五组资源普通和ASan/UBSan各1800帧通过，900手动保持、835次新轨道发布。4096变异/截断输入中168个可接受、3928个明确拒绝，所有路径完成资源清理；3项相关CTest、架构及Switch NVK构建通过。新模块尚未由应用引用，NRO仍b9a44d44b54ff020b7b12ed5921e7c2215f834fab705eab4e9982d945afb6b61。无新GPU/实机或日文EXE逐指令证据，未重复打包。

继续504335/504802选择UI、真实演员/音频、flow8与原标题应用衔接；完整移植未完成，防休眠保持。
