# ADR 0007:M8.2 作物生长

- 日期:2026-06-22
- 状态:已采纳
- 相关:[ADR 0006 时间系统](0006-m8-1-time-system.md)、[M8.2 设计](../superpowers/specs/2026-06-22-m8-2-crop-growth-design.md)

## 背景

M8.1 落地时间系统(`TimeSystem`/`TimeStep`)。M8.2 在其上实现农场核心玩法第二环:作物生长。

## 决策

1. **作物存 `me_domain` 独立 `FarmField`**(以瓦片坐标为键的 `std::map` 网格),非 Scene 组件。沿用 `TimeSystem` 先例:纯 CPU 可 doctest,与 Scene/RHI 解耦;渲染上屏留后续切片。
2. **浇水驱动状态机**:`CropInstance.daysInStage` 计"已浇水天数"而非自然天。`AdvanceDays` 每天仅推进已浇水的作物并清浇水标记,未浇水停滞(不死亡),成熟阶段不再前进。
3. **5 个 Tool**(`crop.get_field`/`plant`/`water`/`advance_days`/`harvest`),Builtin 总数 8→13。作物变更为**运行时游戏态,不经 CommandStack**——沿用 ADR 0006 文档化例外("变更经 Command"是场景编辑约定,不适用运行时态);dry-run 用 `FarmField` 值拷贝副本预演,零副作用。
4. **`crop.harvest` = EditorOnly** 权限梯度:销毁性产出(清空瓦片),与 `scene.destroy_entity` 同档,演示三层白名单;查询 AgentAllowed、plant/water/advance Automation。
5. **收获产出不入库**:`crop.harvest` 把 `{itemId,count}` 作为 `ToolResult` 值返回,不写库存(库存是 M8.3,YAGNI)。
6. **时间与农场解耦**:`crop.advance_days(days)` 显式接受天数;调用方读 `time.advance` 的 `TimeStep.daysCrossed` 再喂入,`time.advance` 不内联农场。
7. **作物表数据驱动**:`assets/config/crops.json` 经 `LoadCropDatabase` 加载校验,取值与单测 `ValidCropJson()` 严格一致;源码零硬编码数值。

## 后果

- 作物生长逻辑全 CPU,WSL doctest 红绿,无 Windows/GPU 依赖。
- 产出未入库、作物未上屏、无季节锁定/枯萎/重生——均为后续里程碑/切片(YAGNI)。
- `FarmField` 值语义可拷贝是 dry-run 零副作用的基石(同 `TimeSystem`)。
