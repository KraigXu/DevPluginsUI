# MetaStory × Metaplot（路线 A）对接说明

本文档记录 **路线 A**（Metaplot 图驱动拓扑，编译结果仍为 MetaStory 运行时）已落地内容与未完成项，便于评审与排期。

---

## 1. 路线定义（简要）

- **路线 A**：编辑与权威拓扑来自 **`UMetaplotFlow`**；编译前同步为「影子」**`UMetaStoryState` 树**，再走现有 **`FMetaStoryCompiler`**，运行时仍为 **`UMetaStory` / Compact 数据**。
- **范围约定**：仅支持 **新建图资产**（不承诺旧树资产自动迁移）；目标为与原有语义 **尽量等价**，当前版本仅部分等价（见第 3 节）。

---

## 2. 已完成工作

### 2.1 插件与构建

- **`Plugins/Metaplot/Metaplot.uplugin`**：移除对 **MetaStory** 的插件依赖（Metaplot 模块未引用 MetaStory，避免与 MetaStory→Metaplot 形成环）。
- **`Plugins/MetaStory/MetaStory.uplugin`**：增加对 **Metaplot** 的插件依赖。
- **`MetaStoryEditorModule.Build.cs`**：增加模块依赖 **`Metaplot`**。

### 2.2 编辑数据模型

- **`UMetaStoryEditorData`**（`MetaStoryEditorData.h`）：
  - **`bUseMetaplotFlowTopology`**：为 true 时以 Metaplot 图为拓扑真源。
  - **`MetaplotFlow`**：`Instanced` 子对象（`UMetaplotFlow`，Outer 为当前 EditorData）。
  - 头文件包含 **`Flow/MetaplotFlow.h`**。

### 2.3 影子状态树同步（核心）

- **`MetaStoryMetaplotTopology.h` / `MetaStoryMetaplotTopology.cpp`**
  - 入口：**`UE::MetaStory::MetaplotTopology::RebuildShadowStates(EditorData, Log)`**
  - 从 **`UMetaplotFlow`** 生成：
    - 根状态 **`MetaplotRoot`**（`Group`，`TrySelectChildrenInOrder`）。
    - 每个 **`FMetaplotNode`** 对应一个 **`UMetaStoryState`**，**`State.ID == NodeId`**（与 Metaplot 节点 GUID 对齐）。
    - 根下子节点顺序：**Start 节点在前**，其余按 Stage/Layer/Guid 排序。
    - **`FMetaplotTransition`** → **`FMetaStoryTransition`**：`OnStateCompleted` + **`GotoState`**。
    - **Parallel 节点**：`TasksCompletion = All`（与「并行需齐」方向对齐，任务内容未桥接时语义仍不完整）。
  - **校验**：过渡端点须存在于 `Nodes`；**Start** 无入边；**入度 > 1**（汇合/合并）当前报错拒绝（见未完成项）。
  - **条件**：`FMetaplotCondition` 尚未下沉为 MetaStory 条件节点，仅 **Warning**。

### 2.4 编译与加载时机

- **`FMetaStoryCompiler::Compile`**：在 **`CreateStates()`** 之前，若启用 Metaplot 拓扑则先 **`RebuildShadowStates`**。
- **`UMetaStoryEditorData::PostLoad`**：若启用拓扑且存在 **`MetaplotFlow`**，先 **`RebuildShadowStates`**，再走原有 **`VisitHierarchy`** 等逻辑。

### 2.5 新建资产工厂

- **`UMetaStoryMetaplotGraphFactory`**（显示名：**MetaStory (Metaplot Graph)**）
  - 创建 **`bUseMetaplotFlowTopology = true`**、内嵌 **`MetaplotFlow`**、带默认 **Start** 节点的 **`UMetaStory`**。
  - **不**调用原有 **`AddRootState()`**；依赖同步生成的影子树参与编译。

### 2.6 编译修复（UE 5.7）

- **`TArray<const FMetaplotNode*>::Sort`**：谓词须使用 **`const FMetaplotNode&`**（引擎对指针数组 Sort 会解引用），不能再用 **`const FMetaplotNode*`**。

---

## 3. 未完成 / 待办（按优先级建议）

| 项目 | 说明 |
|------|------|
| **汇合节点（入度 > 1）** | 当前直接报错；需在影子树或 Compact 层设计汇合语义后再放开。 |
| **FMetaplotCondition → MetaStory** | `RequiredNodeCompleted`、`RandomProbability`、`CustomBehavior` 未映射为 **`FMetaStoryEditorNode`**，过渡条件与 Metaplot 运行时 **不完全等价**。 |
| **任务桥接** | Metaplot **`UMetaplotStoryTask` / FMetaplotEditorTaskNode** 与 MetaStory **`FMetaStoryEditorNode` / FMetaStoryTaskBase** 未对接；影子状态上 **Tasks 为空**（除非手填）。 |
| **LinkedSubtree / LinkedAsset** | Metaplot Flow 侧尚无对等字段；与 MetaStory **链接子树 / 外链资产** 的等价行为 **未实现**。 |
| **编辑器主 UI** | 尚未把 **Metaplot 流程图（如 `SMetaplotFlowGraphWidget`）** 嵌入 MetaStory 编辑器；当前主要依赖 Details 与内嵌 **`MetaplotFlow`** 属性编辑。 |
| **旧资产迁移** | 按约定 **不做**；若日后需要，需单独迁移工具与兼容策略。 |
| **Undo/事务** | 大规模替换 **`SubTrees`** 与影子对象生命周期，与编辑器 Undo 的深度整合 **未专项验证**。 |

---

## 4. 相关文件路径（便于检索）

| 用途 | 路径 |
|------|------|
| 拓扑同步实现 | `Plugins/MetaStory/Source/MetaStoryEditorModule/Private/MetaStoryMetaplotTopology.cpp` |
| 拓扑 API 声明 | `Plugins/MetaStory/Source/MetaStoryEditorModule/Public/MetaStoryMetaplotTopology.h` |
| 编辑数据字段 | `Plugins/MetaStory/Source/MetaStoryEditorModule/Public/MetaStoryEditorData.h` |
| 编译入口 | `Plugins/MetaStory/Source/MetaStoryEditorModule/Private/MetaStoryCompiler.cpp` |
| PostLoad | `Plugins/MetaStory/Source/MetaStoryEditorModule/Private/MetaStoryEditorData.cpp` |
| 图资产工厂 | `Plugins/MetaStory/Source/MetaStoryEditorModule/Public/MetaStoryMetaplotGraphFactory.h`、`.cpp` |
| Metaplot 设计参考 | `Docs/MetaplotDesign.md` |

---

## 5. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-05-05 | 初稿：路线 A 已做/未做梳理，编译通过（UE 5.7）后归档。 |
