# 人物选择聚光灯

正常选择演员h01..05_60各含两个type2聚光灯。舞台m60_00没有LIGH；灯光按原BK3_L分类和mode1分派，不能把缺失聚光灯改成点光或忽略。

`core/lighting`、`model/environment`、`scene/lighting_assets`和Vulkan uniform/shader支持合计最多8个点光/聚光灯。保持原记录、场景选灯与GPU上传分离，雾/镜头仍独立设置，更新灯光不会覆盖它们。

原`421dd4→4260b6`使用节点世界原点作为位置，方向为`(-world[2], -world[6], world[10])`，再执行`522922`带单位长度容差的归一化；不是直接使用LIGH保存的方向或通常的完整向量矩阵乘法。scene每次读取指定的已发布缓存，不自动推进姿态。

GPU使用全锥角的一半计算内外锥余弦；内锥全强度、外锥之外为0，中间按falloff幂次衰减。灯的环境色、漫反射和高光都乘锥形与距离衰减；全局环境光保持独立。公式参考[Microsoft灯光类型](https://learn.microsoft.com/en-us/windows/win32/direct3d9/light-types)、[衰减与聚光因子](https://learn.microsoft.com/en-us/windows/win32/direct3d9/attenuation-and-spotlight-factor)、[环境光公式](https://learn.microsoft.com/en-us/windows/win32/direct3d9/ambient-lighting)，保持既有逐顶点Gouraud计算。方向在GPU参数打包时再次归一化，避免依赖GPU对巨大向量的归一化范围。

h01_61点光的红色强度为1.08。原选灯注册表曾错误要求所有diffuse≤1，现允许大于1的有限正值，整数环境色打包保留原未掩码的移位/OR行为，以原signed32转换范围为上限。模型环境光记录仍需现有0..1支持范围；directional灯、负falloff和退化聚光方向明确不支持。

验证固定中文分析EXE SHA256 a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e：十个实际选择模型，4352次原非环境灯提交、2560份分派绘制快照、2048份超1强度分派，参数逐位相同。该证据捕获D3D提交，不是Windows光栅截图。

Vulkan66组点/聚光/高光案例，涵盖内外锥、软硬边、falloff0/8/分数、8灯/混合灯、范围与距离、光源重合点和异常输入；独立double公式/插值回读与普通/ASan均差≤2/255。雾320透视采样回归通过。90背景720GPU帧、145496实例/6启雾配置普通/ASan一致；十演员1800帧CPU灯光缓存普通/ASan通过。67项host CTest、29项Python、ASan选灯单元、30Hz完整实际应用普通/ASan和Switch NVK构建通过。

源码比已经交付的FPS速率包更新。本阶段不重新发布该包，也不把灯光组件通过当作人物选择页面已接通。下一项实际舞台/相机/演员的forest组合、重载资源生命周期和51ac5d整体；61视频及flow8仍未完成。防休眠继续。
