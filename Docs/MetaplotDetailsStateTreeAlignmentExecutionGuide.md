# Metaplot Details 对齐 StateTree 执行指南（详细版）

## 1. 文档目的

本指南用于把 `Metaplot` 的节点详情与任务编辑逻辑，逐步改造成与 UE `StateTree` 同构的架构与交互。

目标不是“视觉近似”，而是实现以下一致性：

- 使用 `IDetailCustomization + IPropertyHandle` 直接编辑真实资产/编辑数据。
- 使用统一工具层（`EditorNodeUtils` 思路）管理数组、事务、初始化与刷新。
- 任务项采用“编辑节点模型”，而非轻量 spec 结构直编。
- Add Task、项展开、Undo/Redo、保存重开行为稳定且可预测。

---

## 2. 术语与对照

- **现状模型（Metaplot）**：`FMetaplotStoryTaskSpec`（`Task`/`TaskClass`/`bRequired`）
- **目标模型（StateTree思路）**：`FStateTreeEditorNode`（节点定义 + 实例数据 + 元信息）
- **现状详情入口**：`FMetaplotFlowDetailsCustomization`
- **目标详情入口形态**：`FStateTreeStateDetails` 同思路
- **现状工具散落点**：`MetaplotDetailsCustomization.cpp` 内静态函数
- **目标工具层**：`MetaplotEditorNodeUtils.*`

---

## 3. 最终完成标准（Definition of Done）

满足以下全部项，才视为“对齐完成”：

1. 任务详情不再依赖 `FMetaplotStoryTaskSpec` 作为主要编辑单元，而是基于编辑节点模型。
2. 任务数组构建、Add 流程、事务提交流程全部走 `MetaplotEditorNodeUtils`。
3. Add Task 使用专用节点类型选择器（不是手写 `TObjectIterator` 菜单）。
4. 任务项头部与展开行为与 StateTree 同构（至少：结构、流程、事务时机一致）。
5. Undo/Redo、切节点、保存重开、批量编辑均稳定。
6. 样式键来自 `FMetaplotEditorStyle`，不依赖 `StateTreeEditorModule`。

---

## 4. 分阶段改造计划

> 建议分 5 个阶段提交，每个阶段都可独立编译、回归、回滚。

### Phase A：建立目标数据模型（编辑节点模型）

#### A.1 新增编辑节点结构

新增文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotEditorTaskNode.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotEditorTaskNode.cpp`

建议结构（最小）：

- `FGuid ID`
- `TSoftClassPtr<UMetaplotStoryTask> TaskClass`（类型源）
- `TObjectPtr<UMetaplotStoryTask> InstanceObject`（编辑实例，`Instanced`）
- `bool bEnabled`
- `bool bConsideredForCompletion`

> 如果你希望更贴 StateTree，可改为 `FInstancedStruct Node + FInstancedStruct InstanceData` 方案。

#### A.2 把编辑数据挂到资产

在合适的数据容器中（建议编辑器数据对象，次选直接挂 `UMetaplotFlow`）增加：

- `TArray<FMetaplotEditorTaskNode> Tasks`

并建立迁移字段：

- 旧 `StoryTasks` 标记为兼容路径，不再作为主编辑源。

#### A.3 迁移函数（一次性）

新增迁移逻辑：

- `MigrateStoryTaskSpecsToEditorTaskNodes(...)`

规则：

- `TaskClass`/`Task` -> `TaskClass + InstanceObject`
- 缺失 `ID` 则生成新 GUID
- 默认 `bEnabled=true`、`bConsideredForCompletion=true`

验收：

- 旧资产加载后可自动产生新任务节点数组。
- 保存后可稳定重开，不重复脏写。

---

### Phase B：抽离统一工具层（MetaplotEditorNodeUtils）

新增文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotEditorNodeUtils.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotEditorNodeUtils.cpp`

必须提供的函数：

1. `ModifyNodeInTransaction(...)`
2. `MakeArrayCategoryHeader(...)`
3. `MakeArrayItems(...)`
4. `SetNodeType(...)`（按 class 初始化节点与实例）
5. `ConditionalUpdateNodeInstanceData(...)`
6. `InstantiateStructSubobjects(...)`

执行要求：

- 所有数组变更必须统一走：
  - `NotifyPreChange`
  - 修改
  - `NotifyPostChange(EPropertyChangeType::ValueSet)`
  - `NotifyFinishedChangingProperties`
- Add 后默认展开新项：`SetExpanded(true)`

验收：

- `MetaplotDetailsCustomization.cpp` 中不再保留“业务级事务/数组处理”静态函数。

---

### Phase C：替换 Add Task 选择器为 NodeTypePicker 路线

新增文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Customizations/Widgets/SMetaplotNodeTypePicker.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/Customizations/Widgets/SMetaplotNodeTypePicker.cpp`

要求：

- 支持 Schema / BaseClass / BaseStruct 过滤
- 回调统一为 `OnNodeTypePicked(...)`
- 由 `MetaplotEditorNodeUtils::SetNodeType(...)` 完成节点初始化

替换点：

- 当前 `MetaplotDetailsCustomization.cpp` 中 `BuildAddTaskMenu(...)` 手写类列表逻辑

验收：

- Add Task 交互路径与 StateTree 同构（选择器 -> 初始化 -> 展开 -> 刷新）。

---

### Phase D：瘦身 DetailsCustomization（只做编排）

目标文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsCustomization.cpp`

需要保留：

- 分类布局（Node / Tasks / Transition）
- 调用 `MetaplotEditorNodeUtils` 工具函数
- 少量上下文解析（`DetailsContext`）

需要删除：

- 直接操作任务数组的静态函数
- 直接 new task 实例的业务逻辑
- 事务细节代码（迁移到 Utils）

验收：

- Details 文件只剩“UI编排”和极少量 glue code。

---

### Phase E：样式键与资源完全切换到 MetaplotEditorStyle

目标文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Public/MetaplotEditorStyle.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/MetaplotEditorStyle.cpp`

建议补齐键：

- `Metaplot.Category`
- `MetaplotEditor.Tasks`
- `MetaplotEditor.Tasks.Large`
- `MetaplotEditor.TasksCompletion.Enabled`
- `MetaplotEditor.TasksCompletion.Disabled`
- `Metaplot.Task.Title`
- `Metaplot.Task.Title.Bold`
- `Metaplot.Task.Title.Subdued`

资源建议：

- 放在 `Plugins/Metaplot/Resources/Icons`
- 不引用 `FStateTreeEditorStyle::Get()`

验收：

- 禁用 StateTree 插件后，Metaplot 详情样式完全可用。

---

## 5. 文件级改造清单（逐个勾选）

### 5.1 现有文件要改

- [x] `Plugins/Metaplot/Source/Metaplot/Public/Flow/MetaplotFlow.h`
- [x] `Plugins/Metaplot/Source/Metaplot/Private/Runtime/MetaplotInstance.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsCustomization.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotScenarioAssetEditorToolkit.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotScenarioAssetEditorToolkit.h`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/MetaplotEditor.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/MetaplotEditorStyle.cpp`

### 5.2 新增文件

- [x] `Plugins/Metaplot/Source/Metaplot/Public/Scenario/MetaplotEditorTaskNode.h`（主定义）
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotEditorTaskNode.h`（编辑器侧桥接头）
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotEditorTaskNode.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotEditorNodeUtils.h`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotEditorNodeUtils.cpp`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Customizations/Widgets/SMetaplotNodeTypePicker.h`
- [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Customizations/Widgets/SMetaplotNodeTypePicker.cpp`

---

## 6. 每阶段验收与测试用例

### 必测场景（每阶段都跑）

1. 选中节点后，任务区可见且内容正确
2. Add Task 后立即可见并展开
3. 删除任务后 UI 立即刷新
4. 切换节点 -> 切回，数据一致
5. Ctrl+Z / Ctrl+Y 正确
6. 保存资产 + 重开编辑器，数据一致

### 增量场景（Phase C 之后）

7. Picker 过滤无关类
8. 新增任务默认实例化字段正确
9. 多任务批量编辑后无丢项

---

## 7. 风险点与规避

### 风险 1：上下文错位（编辑到非选中节点）

规避：

- 所有写操作先校验 `SelectedNodeId` 与句柄 NodeId 一致
- 无上下文时显示只读提示，不允许写入

### 风险 2：事务边界混乱

规避：

- 严禁在 Details 文件里重复造事务
- 统一通过 `MetaplotEditorNodeUtils::ModifyNodeInTransaction(...)`

### 风险 3：实例对象 Outer 错误导致偶发丢失

规避：

- 新建/复制实例统一 Outer 到资产对象
- 保存前执行一次 Outer 归一化

---

## 8. 回滚策略

### 提交粒度（强制）

- Commit 1：Phase A（数据模型 + 迁移）
- Commit 2：Phase B（工具层）
- Commit 3：Phase C（Picker）
- Commit 4：Phase D（Details 瘦身）
- Commit 5：Phase E（样式完善）

### 回滚建议

- 任一阶段失败，直接回滚该阶段 commit，不跨阶段补丁式修补。

---

## 9. 推荐执行顺序（一天内可落地的版本）

如果你希望先稳定再完美，建议：

1. 先做 Phase B（工具层）+ Phase D（Details瘦身）
2. 再做 Phase C（Picker）
3. 最后做 Phase A（模型升级）与 Phase E（样式完整）

这样可以先解决“稳定性和一致性”，再做“完全同构”。

---

## 10. 当前执行状态记录（维护区）

> 每次改完一批请更新时间、提交号和结论。

- 时间：
- 提交：
- 完成阶段：
- 回归结果：
- 遗留问题：
- 时间：2026-04-27 13:25 (UTC+8)
- 提交：未提交（工作区）
- 完成阶段：Phase B/C/D/E 已落地；Phase A（同构深度）部分完成
- 完成度（按 DoD + 阶段目标）：约 `82%`
- 回归结果：已有记录显示编译通过，且手动回归 Case 1~6 通过；Case 7/8 口径与路径待补齐
- 遗留问题：
  1) UI 细节仍与 StateTree 存在整洁度差异（体验差异）
  2) Case 7（批量编辑）功能口径需统一（是否纳入本轮范围）
  3) Case 8（Transition 不回归）需补可执行测试路径
  4) Phase A 的“完全同构编辑节点模型（`FStateTreeEditorNode` 等价层）”仍需继续推进
  5) 已补充迁移与归一化链路（`bRequired -> bConsideredForCompletion`、任务实例 Outer 归一、缺失 ID/Class 修复、`NodeEditorTaskSets` 与 `Nodes` 自动对齐）；仍需继续推进更高阶同构（如 `FInstancedStruct` 路线）
  6) 已接入 `MetaplotEditorTaskNode` 属性定制：`TaskClass` 变化时自动校正/重建 `InstanceObject`，避免“类与实例不匹配”漂移
  7) 已接入 Phase A 双轨骨架：`FMetaplotEditorTaskNode` 新增 `NodeData/InstanceData(FInstancedStruct)`，并在 `PostLoad/Normalize` 中实现“旧字段 <-> InstancedStruct”同步
  8) 运行时读取已优先走 `NodeData/InstanceData`（旧字段 fallback），双轨模型已从“仅存储”进入“实际执行”

---

## 11. 10 分钟最小手动回归清单（可直接打勾）

> 目标：快速确认 DoD 第 5 条（Undo/Redo、切节点、保存重开、批量编辑稳定）  
> 建议资产：选一个已有 3+ 节点、2+ 连线、每节点可加任务的 MetaplotFlow 资产

### 11.1 用例清单（执行顺序建议）

- [ ] **Case 1 选中节点展示正确**：点击节点 A，Details 的 Node/Tasks 区可见且任务数量与图中一致
- [ ] **Case 2 Add Task 立即展开**：在节点 A 点击 Add Task，新增项出现并自动展开，字段可编辑
- [ ] **Case 3 删除任务即时刷新**：删除刚新增任务，列表立即刷新且数量正确
- [ ] **Case 4 切节点往返一致**：从节点 A 切到节点 B，再切回节点 A，任务数据不丢失不串位
- [ ] **Case 5 Undo/Redo 正常**：对 Add/删除任务各做一次 Ctrl+Z、Ctrl+Y，结果与预期一致
- [ ] **Case 6 保存重开一致**：保存资产、关闭并重开编辑器窗口，任务和展开状态行为正常
- [ ] **Case 7 批量编辑稳定**：连续新增 3 个任务并修改字段后保存重开，无丢项/重复/错序
- [ ] **Case 8 过渡条件编辑不回归**：切到 Transition，编辑条件后返回节点详情，节点任务区仍正常

### 11.2 快速记录模板（复制一份填一份）

```text
[手动回归记录]
时间：
执行人：
资产：
引擎/分支：

Case 1（选中节点展示）：通过/失败 - 备注：
Case 2（Add Task 展开）：通过/失败 - 备注：
Case 3（删除任务刷新）：通过/失败 - 备注：
Case 4（切节点往返）：通过/失败 - 备注：
Case 5（Undo/Redo）：通过/失败 - 备注：
Case 6（保存重开）：通过/失败 - 备注：
Case 7（批量编辑）：通过/失败 - 备注：
Case 8（Transition 不回归）：通过/失败 - 备注：

结论：可签收 / 不可签收
遗留问题：
```

### 11.3 签收建议

- 8 项全部通过：可将“遗留问题”更新为“无”，并标记本指南对齐完成
- 如出现失败：记录复现步骤（最少 3 步）+ 期望/实际，再进入缺陷修复

### 11.4 本轮回归记录（2026-04-27 13:00）

```text
[手动回归记录]
时间：2026-4-27 13
执行人：徐坤垒
资产：MF_Test2
引擎/分支：master

Case 1（选中节点展示）：通过 - 备注：虽通过，但 Details 上显示了 Flow，按预期不应显示
Case 2（Add Task 展开）：通过 - 备注：点击 Add new Task 后，展开界面不如 StateTree 整洁
Case 3（删除任务刷新）：通过
Case 4（切节点往返）：通过
Case 5（Undo/Redo）：通过
Case 6（保存重开）：通过
Case 7（批量编辑）：没有该功能
Case 8（Transition 不回归）：无法测试

结论：可签收（带遗留项）
遗留问题：
1) Details 面板仍出现 Flow 展示项，需确认是否应隐藏
2) Add Task 展开 UI 与 StateTree 的整洁度存在差异（体验差异）
3) Case 7 功能/口径需澄清（无该功能还是入口未暴露）
4) Case 8 缺少可执行测试路径，需补充可复现步骤后回归
```

### 11.5 双轨同构专项手测（Phase A 收尾）

> 目标：验证 `FMetaplotEditorTaskNode` 的“`NodeData/InstanceData` 优先、旧字段 fallback”已进入真实执行路径  
> 建议资产：`MF_Test2` 或任一含 2+ 节点、每节点 1+ Task 的 Flow

- [ ] **Case A1 新字段优先（TaskClass）**：在任务项中修改 `TaskClass`，确认 `InstanceObject` 自动重建为同类实例，并可正常运行
- [ ] **Case A2 新字段优先（开关位）**：切换任务启用状态后运行，确认禁用任务不会进入 `EnterTask`
- [ ] **Case A3 新字段优先（完成参与）**：将任务设为“不参与完成”后运行，确认该任务失败不影响节点聚合结果
- [ ] **Case A4 旧字段 fallback**：构造缺失 `NodeData/InstanceData` 的旧任务数据，确认运行时仍按兼容字段执行
- [ ] **Case A5 归一化自修复**：保存重开后检查 `ID`、`TaskClass`、`InstanceObject` Outer、`NodeEditorTaskSets` 对齐状态

