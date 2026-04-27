# Metaplot Details 对齐 StateTree 方案记录

## 背景

当前 `Metaplot` 的节点详情面板已具备基础任务编辑能力，但与 UE `StateTree` 的任务面板在交互和稳定性上仍有差异，主要体现为：

- 任务项显示层级偏深。
- 任务项偶发不稳定（已通过事务与数组构建方式修复一部分）。
- 实现架构仍依赖 `UMetaplotNodeDetailsProxy`，与 StateTree 的实现思路不一致。

本记录用于明确后续重构目标与实施顺序，作为项目内长期追踪文档。

---

## 目标

将 `Metaplot` 节点详情编辑架构逐步对齐到 StateTree 思路：

- 以 `IDetailCustomization + IPropertyHandle` 直接编辑真实资产。
- 使用统一的数组与事务工具（类似 `EditorNodeUtils`）。
- 最终移除 `UMetaplotNodeDetailsProxy` 这一中间对象层。

---

## 现状（截至本记录）

### 已完成

- 任务分类头与列表已改为接近 StateTree 组织方式。
- 任务数组项构建已切换为 `FDetailArrayBuilder`。
- Add Task 已加入事务与 `NotifyPre/Post/Finished` 提交流程。
- 任务实例存储已升级为：
  - `FMetaplotStoryTaskSpec::Task`（实例对象）
  - `TaskClass`（兼容字段）
- `PushToFlow()` 内补充了实例 Outer 归一化（`FlowAsset`）。
- 节点详情已切换为 `MetaplotFlow` 资产直编（不再经 `UMetaplotNodeDetailsProxy`）。
- `MetaplotNodeDetailsProxy` class layout 已移除，节点详情 customization 已改为基于 `SelectedNodeId` 的 direct handle 路径。
- `MetaplotDetailsProxy.h/.cpp` 已删除；Transition 暂保留为独立 `UMetaplotTransitionDetailsProxy`（后续可继续资产化迁移）。

### 未完成 / 差异点

- 未形成 Metaplot 自己的完整 `EditorNodeUtils` 抽象。
- 任务项逻辑尚非 StateTree 的 `FStateTreeEditorNode` 模型。
- 样式层已引入 `MetaplotEditorStyle`，但仍需继续细化键与资源映射。

---

## 三步改造方案（建议按顺序执行）

## 第 1 步：建立详情上下文通路（低风险）

### 目标

让 customization 可直接拿到真实上下文（`FlowAsset + SelectedNodeId`），为后续去 Proxy 做准备。

### 建议改动

- 在编辑器工具类中维护可访问的详情上下文对象/接口。
- 当前 `DetailsView` 可暂时保留 Proxy 绑定，但同步注入上下文。
- 确保 `CustomizeDetails(...)` 能拿到当前节点选择信息。

### 验收

- 功能行为不变。
- 在 customization 内可稳定访问 `FlowAsset` 与当前节点 GUID。

---

## 第 2 步：先迁移 Tasks 区为“直接资产编辑”

### 目标

优先消除任务区不稳定来源，切换到 StateTree 风格的直接资产路径。

### 建议改动

- Tasks 区不再依赖 `NodeDetailsProxy::StoryTasks`。
- 直接通过 `FlowAsset->NodeTaskSets` + `SelectedNodeId` 做 `IPropertyHandle` 映射。
- 提取 `MetaplotEditorNodeUtils`（建议）：
  - `MakeArrayCategoryHeader(...)`
  - `MakeArrayItems(...)`
  - `ModifyNodeInTransaction(...)`
- Add Task 与数组修改全部经统一事务工具。

### 验收

- 新增/删除任务稳定。
- 切换节点、Undo/Redo、保存重开后任务项一致。
- `StoryTasks` 字段可从 Proxy 中移除。

---

## 第 3 步：移除 Node Proxy，节点基础属性也资产化

### 目标

实现与 StateTree 同形态架构：详情层直接编辑资产，不再中间镜像对象。

### 建议改动

- `DetailsView` 对节点编辑改为绑定真实 `UMetaplotFlow`。
- 节点基础字段（名称、描述、类型、布局、策略）改为 direct handle + selected node path。
- 删除 `UMetaplotNodeDetailsProxy` 及其 class layout 注册。
- Transition 后续可按同模式迁移。

### 验收

- 节点基础字段和任务字段都不依赖 Proxy。
- 代码结构统一为 `DetailCustomization + Handle + Utils`。

---

## 风险与回滚策略

### 风险

- 选择上下文丢失导致详情面板空白或编辑错对象。
- 事务边界不一致导致 Undo/Redo 异常。
- 多选场景下 handle 映射行为不一致。

### 回滚建议

- 每一步单独提交，保持可独立回退。
- 第 2 步前保留 Proxy 代码，不立即删除。
- 每步完成后执行固定回归清单（见下文）。

---

## 每步回归清单

- 选中节点后任务区是否正确显示。
- 新增任务后是否立即可见、可展开编辑。
- 删除任务后是否立即刷新。
- 切换到其他节点再切回，数据是否一致。
- 保存资产、重开编辑器后是否一致。
- Undo/Redo 是否正确恢复任务变更。

### Step 2 回归记录（2026-04-27）

当前回归分为“代码路径静态验证”和“编辑器内交互手测”两部分。

#### 已完成（静态验证）

- [x] Tasks 区已不再依赖 `UMetaplotNodeDetailsProxy::StoryTasks` 字段。
- [x] Tasks 句柄已通过 `FlowAsset->NodeTaskSets + SelectedNodeId` 解析并驱动详情数组。
- [x] Add Task 已改为直接在资产 Outer（`FlowAsset`）上创建任务实例，并写入任务数组。
- [x] Proxy 侧 `StoryTasks` 回写路径已移除，不再覆盖资产任务数据。
- [x] `UMetaplotNodeDetailsProxy` 中 `StoryTasks` 字段已移除。

#### 待完成（编辑器手测）

- [ ] 选中节点后任务区正确显示。
- [ ] 新增任务后立即可见、可展开编辑。
- [ ] 删除任务后立即刷新。
- [ ] 切换节点再切回数据一致。
- [ ] 保存资产并重开编辑器后数据一致。
- [ ] Undo/Redo 能正确恢复任务变更。

---

## 参考（StateTree 关键实现位置）

- `StateTreeStateDetails.cpp`
  - `FStateTreeStateDetails::CustomizeDetails(...)`
- `StateTreeEditorNodeUtils.cpp`
  - `MakeArrayCategoryHeader(...)`
  - `MakeArrayItems(...)`
  - `ModifyNodeInTransaction(...)`

> 备注：参考其思路与流程，不建议直接强耦合 `StateTreeEditorModule`。

---

## 文件/函数级 TODO 拆解

本节将三步方案细化到具体文件与函数，便于直接实施。

### Step 1 对应 TODO（上下文通路）

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotScenarioAssetEditorToolkit.cpp`
  - `UpdateDetailsSelectionContext()`
    - 新增“详情上下文同步”逻辑（`FlowAsset + SelectedNodeId`）。
  - `OnNodeSelectionChanged(...)` / `OnMainGraphNodeSelected(...)`
    - 选择变化时更新上下文并触发 `DetailsView` 刷新。
  - `OnDetailsFinishedChangingProperties(...)`
    - 保留刷新逻辑，确保上下文模式下仍可同步列表与画布。

- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotScenarioAssetEditorToolkit.h`
  - 增加上下文字段（或上下文对象引用）声明。

可选新增（推荐）：

- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotDetailsContext.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsContext.cpp`
  - 承载 `EditingFlowAsset` 与 `SelectedNodeId` 的轻量对象。

---

### Step 2 对应 TODO（Tasks 直接资产编辑）

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsCustomization.cpp`
  - `FMetaplotNodeDetailsProxyCustomization::CustomizeDetails(...)`
    - Tasks 区改为直接操作 `UMetaplotFlow::NodeTaskSets`（通过上下文定位节点）。
  - `AddTaskWithClass(...)`
    - 改为直接写资产数组（而非经过 Proxy 数据中转）。
  - `BuildAddTaskMenu(...)`
    - 保持 class 选择逻辑，动作改为调用统一事务工具。
  - `ConfigureTaskCategoryHeader(...)`
    - 继续保留分类头，后续可挪到工具层统一。

- 新增工具层（建议）：
  - `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotEditorNodeUtils.h`
  - `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotEditorNodeUtils.cpp`
  - 建议提供函数：
    - `MakeArrayCategoryHeader(...)`
    - `MakeArrayItems(...)`
    - `ModifyNodeInTransaction(...)`

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsProxy.cpp`
  - 删除/禁用 `StoryTasks` 的 Proxy 回写路径（仅当 Step 2 完成后）。

- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotDetailsProxy.h`
  - 移除 `StoryTasks` 字段（Step 2 验收后）。

---

### Step 3 对应 TODO（移除 Node Proxy）

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotScenarioAssetEditorToolkit.cpp`
  - `UpdateDetailsSelectionContext()`
    - [x] 节点选中时不再 `SetObject(NodeDetailsProxy)`，改为绑定 `EditingFlowAsset + DetailsContext`。

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsCustomization.cpp`
  - [x] 节点基础字段（Name/Description/Type/Stage/Layer/Policy）已直接映射到资产中的选中节点。
  - [x] 已去除对 `UMetaplotNodeDetailsProxy` 的依赖。

- `Plugins/Metaplot/Source/MetaplotEditor/Private/MetaplotEditor.cpp`
  - [x] 已移除 `MetaplotNodeDetailsProxy` 的 class layout 注册/反注册（改为 `MetaplotFlow`）。

- 删除文件（Step 3 最后执行）：
  - [x] `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotDetailsProxy.h`
  - [x] `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsProxy.cpp`
  - [新增] Transition Proxy 已拆分为独立文件：
    - `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotTransitionDetailsProxy.h`
    - `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotTransitionDetailsProxy.cpp`

---

## 建议提交粒度（便于回滚）

- Commit A：上下文通路（Step 1）
- Commit B：Tasks 资产直编 + 工具层（Step 2）
- Commit C：移除 Node Proxy（Step 3）

每个提交后都执行“每步回归清单”再进入下一步。

