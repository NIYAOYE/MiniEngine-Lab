# me_core

零外部依赖的基础设施层。namespace `me`。

## 内容(M0)
- **数学(2D)**:`Vector2/3/4`、`Matrix4x4`(行主序/行向量,含正交投影)、`Transform2D`、`Rect`、`AABB`、`MathConstants`(具名常量)。
- **句柄**:`Handle<T>`(index + generation,类型安全)。
- **日志/断言**:`Log`(`FormatLogLine` 纯函数 + `ME_LOG_*` 宏)、`Assert`(`ME_ASSERT` / `ME_ASSERT_MSG`,Release 空操作)。

## 约定
- 坐标系:世界空间 **Y 轴向上**,正交投影。
- 矩阵:**行主序存储 + 行向量**(`p' = p * M`,平移在第 4 行),与 DX12/DirectXMath 同源。
- 无异常;不变量违反用 `ME_ASSERT`。

## 依赖
无(下层,任何模块不得被它反向依赖)。
