# phase8 回放父控制（2026-09-30）

完整 `48302B` 父控制器和 `483A36` 动作分类已恢复，`scene/ending_state` 提供直接借用当前进程字段的绑定。普通与 ASan/UBSan 原指令对照及状态/入口/重载回归通过，见 `ending-gallery-control-verification.json`。五个子控制器仍为必需服务，实际 phase8 尚未开放。

## 状态与记录来源

原跳表 `483A02` 的九项已按固定 EXE 字节核对：

| 主状态 | 实际分支 |
| --- | --- |
| 0 | 读取工作区动作、分类、安排阶段/语音 |
| 1 | 返回，仅保留入口camera_request=1写入 |
| 2 | 重载/退出请求，等待speech0后设置curtain_wanted |
| 3 | 四个开场子状态：表情初始化、收窄视角、预设相机/首句、语音等待 |
| 4..8 | 依次调用483AF0/4843AA/4855C9/48758C/488674 |

每次入口先捕获角色记录索引，之后写 `camera_request=1`。开场从这名角色的 `records.groups[group].retained[variant != 0]` 复制40KB工作区；相机回调改变group不能改变本次来源。记录的actions/count不参与这份复制。保持完整原复制/清零及各组记录，工作区越界在实际访问处失败，不编造默认动作。

`483A36` 先保存previous，再根据记录**低字节**分类；后续选择子类型仍比较完整32位字值12/13/14。因此 `0x1000c` 会被分类到同一动作，但不能当作完整值12设置selection。未匹配的有符号字节保持action；selection+1按原32位回绕。

## 顺序与生命周期

- `5545E0` 独立FOV过程值初始1，只按原赋值重置；提交当前FOV后才减去 `seconds * .2f`，严格seconds<1才减，最低 .2，保留unordered分支。相机完成只读取返回值低AL。
- 开场第三步先改子状态和camera_mode，再写语音名、加载、读取当前音量播放，最后清auxiliary.pending。开场speech1区分无缓冲和停止的缓冲，语音选择与播放不合并。
- 过渡到动作4在语音加载前清 `6DDE50`；过渡到动作5在播放完成后才清。服务中修改音量/variant/目标等别名时，后续语句按原位置重新读取。
- `bk_ending_state_gallery_bindings` 直接借用 `retained.final` 工作区、子状态、计数器，以及frame/control/auxiliary/final_state等已有字段，不复制出第二份状态，不执行重置或打开新场景。
- 缺子控制器或其他必需服务明确失败，保留执行前缀。这里只调度实际重载请求，不在父控制器内直接销毁或替换资源。

## 已执行验证

固定 EXE SHA-256：`a3c9360321d4e4687b1a8a095f514c6c38c327119d8f52aafe664f02bc51679e`。

终态目录：`build/validation/ending-gallery-cpu-bkd4cppl/verification.json`，两项配对均passed，909份源/配置摘要在运行中保持一致。每种构建分别通过：

- 12,000随机帧、1,054边界帧、100连续父状态帧；边界覆盖两种variant、256种低字节、额外高位，以及30个回调改变group/variant的记录来源检查。
- 7,922个观察调用、190次live变更、785个服务失败前缀、129次完整记录复制。原CRT memcpy/memset、父分支/表及分类实际执行；五个子控制器、音频、相机和表情是受控边界，不能把其夹具算成子控制器实现。
- 18项容量/查询/参数/绑定拒绝，12种缺服务拒绝；保留完整工作区、原记录和实际 `BkEndingState` 的其余字段。
- 状态摘要普通/ASan完全一致：`1293cab51cfedf0fece8bbcc2540fed8d820306600bb10b2f05c3c52cf14d381`。ASan/UBSan无错误，未启用leak detection。
- 普通与ASan各3项CTest（ending-state/entry/reload）通过；29项Python（含2项架构）、shell语法及diff检查通过。本轮未重跑已结算的效果/表现或GPU套件，也未运行整份历史test-host.sh。

`check_ending_gallery_cpu.py` 默认已包含control；本轮用 `--suites control` 执行专项配对。

## 构建及下一项

指定Mesa NVK commit `5dba7886c56460ff47c3038e323d07b9547d6212` 交叉构建通过，ELF未解析符号0。新父控制在game静态库，尚未被应用引用；绑定函数随现有ending_state对象进入ELF，不表示生产phase8已接入。

NRO SHA-256：`8cf378a42c507737733eeda09fd0f5cc04b9baa3403869e6b7155512ebf3c049`，16,076,856字节，仅构建目录。本轮没有新增实际资产回放、GPU或Switch验收，没有整包或推送。

下一项恢复五个子控制器，首先483AF0，再接入已有父控制/48BCBB/48CC18和跨loader生命周期。入口48D7F2重置已存在，`prepare_final`仍须绑定；自然结束、持久解锁、返回和完整像素/实机验收仍未完成。防休眠64008已实际复核，完整移植结束前保持。
