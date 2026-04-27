# Metaplot 细节面板 UE 原生化改造计划

> 文档定位：早期迁移方案（Proxy 时代历史记录）。
>
> 当前 StateTree 对齐执行请优先参考：  
> **[MetaplotDetailsStateTreeAlignmentExecutionGuide.md](MetaplotDetailsStateTreeAlignmentExecutionGuide.md)**
>
> 说明：本文件中的 `UMetaplotNodeDetailsProxy` / `UMetaplotTransitionDetailsProxy` 方案主要用于回溯背景，不作为当前主实施路径。

## 背景与目标

当前编辑器右侧已接入 `IDetailsView`，但主要显示 `UMetaplotFlow` 全局对象；节点与连线选中后仍依赖独立的手写“节点详情”区域。  
本计划目标是将右侧统一为 UE 原生 Details 工作流，实现“选谁改谁”的上下文式属性编辑体验。

目标体验：

- 选中空白：显示 `UMetaplotFlow`（全局设置）。
- 选中节点：显示节点属性（名称、描述、类型、布局、任务、策略等）。
- 选中连线：显示连线属性（Source/Target、Conditions）。
- 后续可扩展多选。

---

## 现状结论（基于现有代码）

- 已创建 `DetailsView`，并挂载在 `Details` Tab。
- 初始化时固定 `DetailsView->SetObject(EditingFlowAsset)`。
- 节点/连线选中已存在回调链（图表与列表），但未驱动 Details 上下文切换。
- 仍存在手写 `NodeDetails` Tab，造成状态分散与重复信息。

---

## 方案总览

### 1) 引入 Editor Proxy（编辑代理对象）

由于节点/连线数据是 `UMetaplotFlow` 的 `USTRUCT` 数组元素，直接用 Details 精准编辑选中项不够稳定，建议引入 Editor-only `UObject` 代理：

- `UMetaplotNodeDetailsProxy`
  - 持有 `FlowAsset` 引用与 `NodeId`
  - 暴露节点可编辑字段
  - 在 `PostEditChangeProperty` 中回写 `Flow->Nodes`
- `UMetaplotTransitionDetailsProxy`
  - 持有 `FlowAsset` 与连线标识（建议 `SourceNodeId + TargetNodeId`）
  - 暴露 `Conditions` 等可编辑字段
  - 在 `PostEditChangeProperty` 中回写 `Flow->Transitions`

### 2) 选择联动 Details 上下文

在现有选择回调中统一切换 Details 对象：

- 选中节点 -> `DetailsView->SetObject(NodeProxy)`
- 选中连线 -> `DetailsView->SetObject(TransitionProxy)`
- 取消选中 -> `DetailsView->SetObject(EditingFlowAsset)`

### 3) 引入 Detail Customization（UE 风格关键）

- `IDetailCustomization`（NodeProxy）
  - 分类组织（General/Layout/Tasks/Runtime）
  - NodeId 只读、Runtime 字段按需只读或隐藏
  - 布局字段加入规则提示
- `IDetailCustomization`（TransitionProxy）
  - Source/Target 友好展示（节点名 + Guid）
  - Conditions 列表分组
- `IPropertyTypeCustomization`（`FMetaplotCondition`）
  - 根据 `Type` 动态显示关联字段
  - `BlackboardKey` 下拉来源于 `DefaultBlackboard`

### 4) 事务与刷新统一

- 属性修改走 `Modify + Transaction + MarkPackageDirty`
- 将图表刷新和 Details 刷新集中到统一入口，避免多处 `ForceRefresh` 导致抖动或重入
- 校验失败时提供可恢复行为（回滚或提示并拒绝）

### 5) 逐步下线手写 NodeDetails 面板

- 过渡期保留为只读提示或快捷按钮区
- 稳定后合并为单一 Details 入口，减少重复维护

---

## 分阶段实施计划

### Phase 1（最小可用）

目标：跑通“选中节点/连线即切换到对应 Details”。

- 新增 Node/Transition Proxy 类
- 在 Toolkit 选择事件中接入 `SetObject(...)`
- 保持现有图表交互与数据结构不变
- 验证 Undo/Redo 与脏标记

### Phase 2（体验升级）

目标：达到 UE 原生 Details 的可用体验。

- 注册 Node/Transition `IDetailCustomization`
- 注册 `FMetaplotCondition` 的 `IPropertyTypeCustomization`
- 优化条件编辑展示逻辑与字段可见性

### Phase 3（收敛与稳定）

目标：收敛刷新链路并清理旧面板。

- 统一刷新入口与事务边界
- 弱化或移除手写 `NodeDetails` Tab
- 清理重复 UI 文本和状态源

### Phase 4（可选增强）

目标：进一步对齐 StateTree 风格任务编辑体验。

- 评估将任务从 class spec 升级到 instanced task（`Instanced/EditInlineNew`）
- 设计迁移策略与兼容方案

---

## 任务拆解清单（建议顺序）

1. 新建编辑代理对象头/源文件（Editor 模块）
2. 在 Toolkit 中持有代理实例并完成生命周期管理
3. 改造节点/连线选择回调 -> 切换 Details 上下文
4. 增加统一 `RefreshEditorView()`（图表、列表、Details）
5. 接入 Node/Transition 详情定制
6. 接入 Condition 属性定制
7. 清理/降级旧 NodeDetails 区域
8. 完成回归验证与文档更新

---

## 风险与规避

- **数组索引不稳定**：连线定位避免仅使用索引，优先使用源/目标 Guid 键。
- **刷新重入**：集中刷新入口，增加防抖/重入保护标记。
- **事务不完整**：统一通过代理写回并覆盖所有可编辑路径。
- **双面板状态冲突**：过渡期明确单一“真值源”是 Details + Flow 数据。

---

## 验收标准

- 图表或列表选中节点时，右侧 Details 可直接编辑该节点并实时反映到画布。
- 选中连线时，Conditions 编辑可用且保存正确。
- Undo/Redo 对 Details 修改与图表表现一致。
- 不再依赖手写节点详情文本区承载核心编辑能力。
- `README.md` 有明确入口链接到本计划文档。
