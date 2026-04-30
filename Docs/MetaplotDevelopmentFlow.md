# Metaplot 开发流程文档

## 1. 文档定位

本文档用于说明 `Metaplot` 插件在本工程中的日常开发流程。它不是主设计规格，设计目标、架构原则与路线图以 [MetaplotDesign.md](MetaplotDesign.md) 为准。

本文档重点回答：

- 新开发者如何启动并理解工程
- Runtime、Editor、示例工程分别如何开发
- 一个功能从需求到验证应如何落地
- 当前阶段的推荐开发顺序与完成标准
- 常见问题如何排查

## 2. 工程结构

```text
DevPluginsUI/
  DevPluginsUI.uproject
  README.md
  Docs/
    MetaplotDesign.md
    MetaplotDevelopmentFlow.md
  Source/
    DevPluginsUI/
      MetaplotSampleStoryTasks.*
  Plugins/
    Metaplot/
      Metaplot.uplugin
      Source/
        Metaplot/
          Public/
          Private/
        MetaplotEditor/
          Public/
          Private/
```

核心路径：

- `Plugins/Metaplot/Source/Metaplot`：Runtime 模块。
- `Plugins/Metaplot/Source/MetaplotEditor`：Editor 模块。
- `Source/DevPluginsUI`：示例工程模块，用于验证插件能力。
- `Docs/MetaplotDesign.md`：主设计文档。

## 3. 开发前准备

### 3.1 Unreal Engine 版本

当前工程文件 `DevPluginsUI.uproject` 的 `EngineAssociation` 为 `5.7`。

README 中仍保留 `5.3+` 的适用范围描述。开发时以实际工程关联版本为准；如果需要声明兼容范围，应在完成真实编译验证后再更新 README。

### 3.2 推荐打开方式

1. 使用 Unreal Editor 打开 `DevPluginsUI.uproject`。
2. 如果提示重新生成工程文件，允许生成。
3. 使用 Visual Studio 或 Rider 打开 `DevPluginsUI.sln`。
4. 编译 Editor Target：`DevPluginsUIEditor`。

### 3.3 开发前检查

每次开始功能开发前建议确认：

- `Plugins/Metaplot/Metaplot.uplugin` 中 `Metaplot` 与 `MetaplotEditor` 两个模块都存在。
- `Metaplot` 模块不要依赖 Editor-only 模块。
- `MetaplotEditor` 可以依赖 `Metaplot`。
- 测试用 `StoryTask` 放在示例工程模块，不直接塞进插件 Runtime，除非它是插件内置能力。

## 4. 模块职责

### 4.1 Runtime 模块：`Metaplot`

Runtime 负责与游戏运行相关的最小核心能力。

主要职责：

- 定义 `UMetaplotFlow` 资产数据结构。
- 定义节点、连线、条件、任务等基础模型。
- 提供 `UMetaplotStoryTask` 生命周期接口。
- 提供 `UMetaplotInstance` 流程执行器。
- 提供 `UMetaplotSubsystem` 作为运行时启动与 Tick 入口。

开发原则：

- 不引用 `UnrealEd`、`PropertyEditor`、`Slate`、`AssetTools` 等 Editor-only 模块。
- 不把编辑器交互状态写入 Runtime 运行逻辑。
- Runtime 数据结构修改要考虑旧资产迁移。
- 涉及多人和复制的字段，应提前区分“资产配置”和“运行实例状态”。

### 4.2 Editor 模块：`MetaplotEditor`

Editor 负责资产创建、图编辑、Details 面板和编辑器交互。

主要职责：

- 注册资产类型动作与资产工厂。
- 打开 `UMetaplotFlow` 的独立资产编辑器。
- 绘制并交互编辑流程图。
- 提供 Details 定制。
- 提供任务类型选择、条件编辑、事务、刷新等编辑器能力。

开发原则：

- 优先直接编辑真实资产。
- 使用 `FScopedTransaction`、`Modify`、`MarkPackageDirty` 保证 Undo/Redo 和资产保存。
- 图编辑和 Details 修改应走一致的刷新入口。
- 不把临时 UI proxy 扩散成长期主模型；当前 Transition proxy 是待收敛点。

### 4.3 示例工程模块：`DevPluginsUI`

示例工程用于验证插件，而不是承载插件核心。

适合放置：

- 示例 `UMetaplotStoryTask`。
- 测试地图。
- 示例蓝图。
- 用于人工验证的内容资产。

不适合放置：

- 插件核心数据结构。
- 编辑器主 UI。
- Runtime 主执行逻辑。

## 5. 标准功能开发流程

### 5.1 需求确认

开始编码前先确认需求属于哪一类：

- 数据模型变更：例如新增条件类型、节点字段、任务配置字段。
- Runtime 变更：例如流程推进、任务执行、与游戏侧共享状态的协作。
- Editor 变更：例如图编辑、Details 面板、任务选择器。
- 示例验证：例如新增测试 StoryTask 或示例资产。
- 文档更新：例如同步路线图、完成标准、限制说明。

如果功能横跨多类，按以下顺序推进：

1. 数据模型
2. 资产迁移/规范化
3. Runtime 消费逻辑
4. Editor 编辑入口
5. 示例验证
6. 文档更新

### 5.2 设计落点

实现前需要明确：

- 字段放在资产上，还是运行实例上。
- 是否需要支持旧资产迁移。
- 是否需要蓝图可见。
- 是否需要 Undo/Redo。
- 是否会影响节点图显示。
- 是否会影响多人复制设计。

建议在代码注释中只写必要的迁移或复杂逻辑说明，不写重复代码含义的注释。

### 5.3 编码顺序

推荐顺序：

1. 修改 Runtime 数据结构。
2. 补齐 `PostLoad` 或 Normalize 逻辑。
3. 修改 Runtime 执行逻辑。
4. 修改 Editor Details 或图交互。
5. 修改示例任务或示例资产。
6. 编译并人工验证。
7. 更新文档。

这样可以避免 Editor 先行后发现 Runtime 数据模型承接不了。

### 5.4 完成标准

一个功能只有满足以下条件才算完成：

- 资产能保存并重新打开。
- Undo/Redo 表现合理。
- Details 和图表显示一致。
- Runtime 不依赖 Editor 模块。
- 旧资产不崩溃，必要时能迁移。
- 文档中的状态描述已同步。

## 6. Runtime 开发流程

### 6.1 修改资产数据结构

入口文件：

- `Plugins/Metaplot/Source/Metaplot/Public/Flow/MetaplotFlow.h`
- `Plugins/Metaplot/Source/Metaplot/Private/Flow/MetaplotFlow.cpp`

流程：

1. 在 `MetaplotFlow.h` 中添加字段或枚举。
2. 判断是否需要 `UPROPERTY`。
3. 判断是否需要 `BlueprintType` / `BlueprintReadWrite`。
4. 如果影响旧资产，补充 `PostLoad`、迁移或 Normalize 逻辑。
5. 确保新增字段有合理默认值。

注意事项：

- `FGuid` 字段默认无效，使用前必须检查 `IsValid()`。
- 新增数组时要考虑节点删除后的同步清理。
- 不要把运行态字段随意写回资产，除非它本来就是编辑态数据。

### 6.2 修改流程执行

入口文件：

- `Plugins/Metaplot/Source/Metaplot/Public/Runtime/MetaplotInstance.h`
- `Plugins/Metaplot/Source/Metaplot/Private/Runtime/MetaplotInstance.cpp`

当前执行链：

```text
Initialize
  -> Start
  -> ActivateNode
  -> BuildNodeTasks
  -> TickInstance
  -> EvaluateActiveNode
  -> ComputeNodeResult
  -> EvaluateTransitionConditions
  -> ActivateNode 或停止
```

修改流程执行时要检查：

- 空资产、无起始节点、无效节点是否安全返回。
- 任务失败、无任务节点、无出边节点如何处理。
- 条件不满足时流程是停止、等待，还是尝试其他边。
- 多条 Transition 同时满足时选择顺序是否明确。

当前默认行为：

- 节点任务全部不再 Running 后，节点完成。
- Transition 按数组顺序尝试。
- 找到第一条满足条件的 Transition 后立即进入目标节点。
- 没有满足的 Transition 时流程停止。

### 6.3 流程共享状态（游戏侧）

插件不再内置流程级键值黑板。若需要跨节点共享数据：

- 在 `DevPluginsUI` 或游戏模块中定义存储（`Subsystem`、`GameInstance`、组件等）。
- 在 `UMetaplotStoryTask` 子类中通过外部引用、`GetOuter()` 链或接口获取该存储。
- 条件与 Transition 若需读取共享状态，应通过新增 `EMetaplotConditionType` 或自定义任务/回调在游戏侧实现，而不是向 `UMetaplotInstance` 加键值 API。

### 6.4 修改 StoryTask

任务基类：

- `Plugins/Metaplot/Source/Metaplot/Public/Runtime/MetaplotStoryTask.h`

生命周期：

```text
EnterTask
  -> TickTask, repeated
  -> ExitTask
```

开发规则：

- `EnterTask` 用于初始化任务运行状态。
- `TickTask` 返回 `Running`、`Succeeded` 或 `Failed`。
- `ExitTask` 用于收尾。
- 示例任务优先放在 `Source/DevPluginsUI`。
- 插件内置任务只有在确认为通用能力时才放进 `Metaplot` 模块。

## 7. Editor 开发流程

### 7.1 资产编辑器入口

入口文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotScenarioAssetEditorToolkit.cpp`
- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotScenarioAssetEditorToolkit.h`

主要职责：

- 初始化资产编辑器布局。
- 创建主图表 Tab 和 Details Tab。
- 处理节点、连线、自动布局等命令。
- 维护选中节点和选中连线。
- 驱动 Details 刷新。

修改建议：

- 新增编辑命令时，在 Toolkit 中完成资产事务和状态同步。
- 图控件只负责交互输入和绘制，不直接承担复杂资产业务规则。
- 资产变更后统一调用刷新逻辑。

### 7.2 图编辑控件

入口文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotFlowGraphWidget.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotFlowGraphWidget.cpp`

主要职责：

- 绘制网格、节点、连线、拖拽预览。
- 命中测试节点和引脚。
- 处理鼠标选择、拖拽、建线、删除。
- 通过 Delegate 把资产修改请求交给 Toolkit。

开发规则：

- 图控件不直接保存资产。
- 涉及 `Modify`、Transaction、`MarkPackageDirty` 的逻辑放在 Toolkit。
- 新增图交互时，同时考虑键盘、右键菜单和鼠标拖拽路径。

### 7.3 Details 定制

入口文件：

- `Plugins/Metaplot/Source/MetaplotEditor/Private/Scenario/MetaplotDetailsCustomization.cpp`
- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotDetailsCustomization.h`
- `Plugins/Metaplot/Source/MetaplotEditor/Public/Scenario/MetaplotDetailsContext.h`

当前主路径：

```text
DetailsView -> UMetaplotFlow
             -> DetailsContext.SelectedNodeId
             -> 定位 Nodes 数组中的当前节点
             -> 定位 NodeEditorTaskSets 中当前节点任务
```

开发规则：

- 节点详情应继续保持资产直编。
- Transition 详情当前还使用 proxy，后续建议迁移到资产直编。
- 添加任务、条件等数组编辑时，优先使用 PropertyHandle。
- 动态显示字段时，使用 `Visibility` 或 PropertyTypeCustomization。

### 7.4 Undo/Redo 与刷新

资产编辑操作应包含：

```cpp
const FScopedTransaction Transaction(LOCTEXT("TransactionName", "Readable Transaction Name"));
EditingFlowAsset->Modify();
// mutate asset
EditingFlowAsset->MarkPackageDirty();
RefreshFlowLists();
UpdateDetailsSelectionContext();
```

注意：

- 子对象或 Instanced Object 修改时，也可能需要对对应对象调用 `Modify()`。
- Details 内通过 `PropertyHandle` 修改时，优先使用编辑器属性系统提供的事务能力。
- 图表与 Details 修改同一份数据后，应保持刷新入口一致。

## 8. 当前重点开发路线

推荐按以下顺序推进。

### 8.1 前置条件模型与 UI

目标：

- `FMetaplotCondition` 可稳定编辑。
- 条件类型切换后只显示相关字段。
- 条件类型切换后，各类型对应字段显示正确（无已移除的黑板比较类型时则不再引用）。
- 条件修改后图表、Details 和资产保持一致。

涉及文件：

- `MetaplotFlow.h`
- `MetaplotInstance.cpp`
- `MetaplotDetailsCustomization.cpp`
- `MetaplotTransitionDetailsProxy.*`

完成标准：

- 可添加、删除、修改条件。
- 重新打开资产后条件不丢失。
- Runtime 能消费条件。
- Undo/Redo 可用。

### 8.2 完整 Undo/Redo 与统一刷新

目标：

- 节点、连线、任务、条件都进入事务链。
- 图和 Details 的状态同步。
- 减少分散 `ForceRefresh`。

涉及文件：

- `MetaplotScenarioAssetEditorToolkit.cpp`
- `MetaplotDetailsCustomization.cpp`
- `MetaplotEditorNodeUtils.*`

完成标准：

- Ctrl+Z / Ctrl+Y 后节点图、Details、资产保存状态一致。
- 连续撤销不会造成选中状态指向无效对象。

### 8.3 StoryTask 发现与注册

目标：

- 编辑器能稳定枚举 `UMetaplotStoryTask` 子类。
- C++ 与蓝图任务都可被选择。
- Add Task 不依赖硬编码示例类。

涉及文件：

- `SMetaplotNodeTypePicker.*`
- `MetaplotDetailsCustomization.cpp`
- 可能涉及 Asset Registry 或 Class Viewer。

完成标准：

- 新增一个 C++ Task 后能在编辑器中选择。
- 新增一个蓝图 Task 后能在编辑器中选择。
- 选择后能创建对应实例并保存到资产。

### 8.4 编辑器内模拟运行

目标：

- 不进入 PIE 也能验证流程。
- 可查看当前节点与任务状态（共享状态由游戏侧展示时可另行对接）。
- 支持手动步进或自动 Tick。

建议方案：

- 在 Editor 模块创建轻量预览运行器。
- 复用 `UMetaplotInstance`，但避免污染真实资产运行态。
- 图上增加运行态高亮。
- Details 或侧栏显示当前任务状态（可选：侧栏显示游戏侧共享状态）。

完成标准：

- 可从 Start 节点开始模拟。
- 可暂停、继续、重置。
- 节点与任务推进可见。
- 不修改资产运行态字段。

### 8.5 Details / StateTree 对齐收尾

目标：

- Transition 详情迁移到资产直编路径。
- 双轨任务模型继续收敛。
- 任务编辑体验更接近 StateTree。

完成标准：

- 节点和连线详情使用一致的数据访问方式。
- proxy 不再是主链路必要对象。
- 旧资产仍可正常打开。

### 8.6 网络复制与多人实例策略

目标：

- 明确哪些状态复制。
- 明确全局流程和玩家私有流程的实例策略。
- 明确服务器权威推进和客户端表现同步边界。

建议在前面编辑器和 Runtime 稳定后再做。

## 9. 验证流程

### 9.1 编译验证

每次修改 C++ 后至少验证：

- `DevPluginsUIEditor` 能编译。
- Unreal Editor 能打开工程。
- Metaplot 插件能加载。

### 9.2 资产编辑验证

建议创建或打开一个 `UMetaplotFlow` 资产，验证：

- 新增节点。
- 删除节点。
- 移动节点。
- 创建连线。
- 删除连线。
- 修改节点 Details。
- 添加任务。
- 修改任务参数。
- 保存资产。
- 关闭并重新打开资产。

### 9.3 Runtime 验证

建议准备一个简单流程：

```text
Start
  -> Delay Task
  -> Inc Int Task
  -> Set String Task
  -> Terminal
```

验证点：

- `StartMetaplotInstance` 返回有效实例。
- `TickInstance` 能推进任务。
- 任务推进与 Transition 条件按预期生效。
- 流程结束后实例被 Subsystem 清理。

### 9.4 条件验证

建议覆盖：

- RequiredNodeCompleted 满足和不满足。
- RandomProbability 为 `0.0` 和 `1.0`。
- 无效 RequiredNodeId。

### 9.5 Undo/Redo 验证

建议覆盖：

- 新增节点后撤销。
- 删除节点后撤销。
- 移动节点后撤销。
- 新增连线后撤销。
- 修改 Details 字段后撤销。
- 添加任务后撤销。
- 修改条件后撤销。

## 10. 文档更新流程

功能开发完成后，根据影响范围更新文档。

必须更新：

- 功能状态变化影响路线图时，更新 `Docs/MetaplotDesign.md`。
- 新增开发流程或约定时，更新本文档。
- README 中项目状态、版本、完成度发生变化时，更新 `README.md`。

文档口径：

- `MetaplotDesign.md` 写“是什么、为什么、目标状态”。
- `MetaplotDevelopmentFlow.md` 写“怎么开发、怎么验证、怎么排查”。
- `README.md` 写“项目当前状态和入口链接”。

## 11. Git 与资产注意事项

### 11.1 源码

提交前检查：

- 没有把临时调试代码留在主路径。
- 没有把 Editor-only include 放进 Runtime 模块。
- 没有无关格式化大改。
- 没有误删用户未确认的文件。

### 11.2 Unreal 资产

`.uasset` 和 `.umap` 是二进制文件，提交前要确认：

- 修改确实与本次功能相关。
- 没有因为打开编辑器造成无关资产脏写。
- 示例资产变更能被复现和说明。

### 11.3 中间目录

通常不提交：

- `Binaries`
- `Intermediate`
- `DerivedDataCache`
- `Saved`
- `.vs`
- `.idea`

如果这些目录出现变更，应优先检查 `.gitignore`。

## 12. 常见问题排查

### 12.1 插件无法加载

检查：

- `Metaplot.uplugin` 模块声明是否正确。
- Runtime 模块是否误依赖 Editor-only 模块。
- Build.cs 依赖是否缺失。
- 编译错误是否发生在生成代码阶段。

### 12.2 Details 不刷新

检查：

- `DetailsContext.SelectedNodeId` 是否正确。
- 选中节点是否仍存在。
- 是否调用了 `UpdateDetailsSelectionContext()`。
- `DetailsView->SetObject()` 是否对象未变导致需要 `ForceRefresh()`。

### 12.3 图表和 Details 数据不一致

检查：

- 是否修改了资产但没有 `MarkPackageDirty()`。
- 是否图控件内部缓存没有刷新。
- 是否节点删除后没有同步 `NodeEditorTaskSets`。
- 是否 Transition proxy 没有把数据写回真实资产。

### 12.4 Undo/Redo 异常

检查：

- 修改前是否调用 `Modify()`。
- 是否创建了 `FScopedTransaction`。
- 是否子对象也需要 `Modify()`。
- 是否有修改绕过了 PropertyHandle。

### 12.5 Runtime 不推进

检查：

- `StartNodeId` 是否有效。
- 起始节点是否存在。
- 当前节点任务是否一直返回 `Running`。
- 所有 Transition 条件是否都不满足。
- 自定义条件或游戏侧状态是否与 Transition 预期一致（若已接入）。

## 13. 开发检查清单

功能开始前：

- 明确影响 Runtime、Editor、示例或文档。
- 明确数据归属。
- 明确是否需要旧资产迁移。

编码中：

- Runtime 不引入 Editor 依赖。
- 资产变更走事务。
- 图和 Details 共用刷新路径。
- 新字段有默认值。

完成前：

- 编译通过。
- 资产可保存并重开。
- Undo/Redo 关键路径可用。
- 示例或人工验证完成。
- 文档同步。

## 14. 当前推荐下一步

短期建议优先做：

1. 完成 Transition 条件编辑的产品化闭环。
2. 收敛 Undo/Redo 和刷新入口。
3. 完成 StoryTask 发现与选择体验。
4. 增加编辑器内模拟运行。

这些完成后，再推进网络复制和多人实例策略会更稳，因为那时数据模型、编辑入口和验证闭环已经比较可靠。
