# Metaplot 事件流程编辑器设计文档

## 1. 文档定位

本文档是 `Metaplot` 的唯一主设计文档，用于统一以下内容：

- 项目定位与设计目标
- 当前实现状态与进度口径
- 编辑器 / Runtime / 网络 / 扩展性设计
- Details 面板对齐 StateTree 的最终结论

此前分散在 `README.md` 与多份 Details 改造计划中的有效信息已合并到本文档，旧计划文档已删除，不再作为主实施依据。

## 2. 项目定位

- 名称：`Metaplot`
- 形态：Unreal Engine `5.3+` 编辑器扩展插件
- 当前插件版本：`0.1.0`（Beta）
- 核心定位：模拟事物发展过程，支持线性、非循环、单向事件流，适用于多人游戏叙事与玩法流程设计

典型场景包括：

- 天气变化序列
- 任务链推进
- 世界状态演变

相较蓝图、`Level Sequence`、`Timeline` 等现有方案，Metaplot 更强调“多阶段、可视化、带条件推进、多人友好”的事件流程表达。

## 3. 设计原则

- 单向无环：流程严格向前推进，不允许循环依赖
- 视觉清晰：采用类似《文明6》科技树的网格化分层布局
- 事件驱动：节点定义进入条件与进入后的任务，由运行时推进
- 多人就绪：以服务器权威与可复制状态为目标
- 高扩展性：通过任务接口承载具体玩法逻辑

## 4. StateTree 对齐结论

`v2.1` 的核心方向，是让节点执行语义向 UE `StateTree` 靠拢。

- 节点从“单行为节点”升级为“状态节点（State）”
- 每个节点可挂载多个 `StoryTask`
- 任务支持 `Enter` / `Tick` / `Exit` 生命周期
- 节点可按策略聚合任务结果，再决定是否允许流转
- 连线条件可读取前序节点结果等数据；流程级共享状态由游戏侧自行承载（插件不再内置键值黑板）

Details 架构的阶段性结论如下：

- 节点详情已切换为 `MetaplotFlow + DetailsContext(SelectedNodeId)` 的资产直编路径
- `UMetaplotNodeDetailsProxy` 已从节点详情主链路移除
- `Transition` 当前仍保留独立 proxy，可在后续继续资产化迁移
- Tasks 区已改为更接近 StateTree 的 `DetailCustomization + PropertyHandle + ArrayBuilder` 路线
- `MetaplotEditorNodeUtils`、`SMetaplotNodeTypePicker`、`MetaplotEditorStyle` 已进入主链路
- 当前专项完成度约 `90%`

尚未完全对齐的点：

- 任务模型仍处于“双轨过渡”形态，尚未达到 `FStateTreeEditorNode` 等价深度
- 部分 UI 整洁度与交互细节仍有收尾空间
- Transition 详情仍未完全统一到资产直编路径

## 5. 当前实现状态

标记说明：`[x] 已完成`、`[~] 部分完成`、`[ ] 未完成`

### A. 基础框架

- [x] 插件双模块结构（`Metaplot` Runtime + `MetaplotEditor` Editor）已建立并可加载
- [x] `Metaplot` 资产类别、资产类型动作注册、资产工厂已实现
- [x] 自定义资产模型已对齐 `UMetaplotFlow`
- [~] 独立编辑器窗口骨架已实现，流程图表与流程设置界面已接入主链路

### B. 编辑器能力

- [~] 节点创建、删除、属性编辑已具备最小可用能力
- [~] 连线读写、拖拽建线、基础校验、lane 分流与 bundling 已具备
- **以下项已明确移出开发计划，不再作为交付目标：**
  1. 前置条件专用编辑器（不再单独做一套条件编辑 UI）
  2. 自动对齐、拓扑布局、子流程折叠
  3. 行为蓝图下拉、新建蓝图、蓝图拖拽建点
- [ ] 节点/连线事务级 Undo/Redo 未完整完成

### C. 运行时能力

- [~] `UMetaplotSubsystem` 与 `UMetaplotInstance` 已具备最小骨架
- [~] `FMetaplotNode`、`FMetaplotTransition`、`FMetaplotCondition` 已具备基础声明
- [~] `UMetaplotStoryTask` 生命周期接口已建立
- [~] 蓝图运行时 API 已有最小入口
- **子流程运行时编排已移出开发计划**（不再实现嵌套子流程的自动编排与回切）

### D. 校验、调试与多人网络

- [~] 实时校验已部分实现
- [ ] 编辑器内模拟运行未完成
- [ ] 游戏内调试命令与日志体系未完成
- [ ] 网络复制、Server/Multicast RPC、多人实例策略未完成

### 进度口径

- README 全量目标（等权清单法）：约 `35%`
- README 全量目标（章节加权法）：约 `31%`
- Details 对齐 StateTree 专项：约 `90%`

说明：

- `90%` 仅代表 Details 专项，不代表 Metaplot 全项目完成度
- 全项目仍处于“可编辑脚手架 + 最小 Runtime”阶段

## 6. 最近迭代结论

截至 `2026-04-27`，与 Details / StateTree 对齐相关的关键落地结论如下：

- `UMetaplotStoryTask`、`UMetaplotInstance`、`UMetaplotSubsystem` 的最小骨架已接入
- `UMetaplotFlow` 已增加 `NodeTaskSets` 与任务聚合相关字段
- 已移除插件内置黑板；共享状态由任务通过 `UMetaplotInstance` 上下文与游戏侧对象协作实现
- 节点增删与 `NodeTaskSets` 已自动同步
- 节点详情已支持“聚焦当前节点任务配置”
- 示例 StoryTask 放在测试项目模块 `Source/DevPluginsUI`
- `FMetaplotEditorTaskNode` 已进入双轨模型：
  - 兼容旧字段
  - 新增 `NodeData / InstanceData`
  - 运行时优先读取新路径，旧字段作为 fallback

## 7. 整体架构

### 7.1 插件组成

- Editor 模块：提供 `UMetaplotFlow` 资产、独立编辑器窗口、图表绘制与详情定制
- Runtime 模块：提供实例管理、Tick 驱动、任务执行与流转能力

### 7.2 自定义资产

`UMetaplotFlow` 继承自 `UObject`，保存：

- `Nodes`
- `Transitions`
- `StartNodeId`

### 7.3 编辑器窗口

当前编辑器主结构已收敛为两块：

- 流程图表
- 流程设置界面（原 Details）

已具备：

- 网格背景
- 节点卡片绘制
- 圆角折线连线
- 左右引脚拖拽建线
- 节点拖拽换位
- 放置区域高亮
- 中键平移与底部横向滚动条

尚未具备：

- 独立节点模板库侧栏
- 更完整的批量图编辑语义

### 7.4 运行时结构

- 服务器：运行 `UMetaplotInstance`，推进节点并广播状态
- 客户端：接收复制状态并表现当前事件，不直接改流程

## 8. 视觉与交互设计

### 8.1 布局

- `X` 轴表示阶段推进顺序（`StageIndex`）
- `Y` 轴表示并行域（`LayerIndex`）
- 节点按网格坐标布局，适合表现单向分阶段流程

### 8.2 节点样式

- 节点类型：`Start` / `Normal` / `Conditional` / `Parallel` / `Terminal`
- 目标状态色：
  - 灰：锁定
  - 蓝：可用
  - 绿：已完成
  - 橙：进行中

### 8.3 连线样式

- 圆角折线路径 + 箭头
- 支持 lane 分流与长跨度 bundling
- 支持关联高亮
- 拒绝无效连接：自连、右往左、同行跨列、重复、成环

## 9. 核心数据模型

### 9.1 `FMetaplotNode`

- `FGuid NodeId`
- `FText NodeName`
- `FText Description`
- `EMetaplotNodeType`
- `StageIndex`
- `LayerIndex`
- 故事任务列表
- 完成策略
- 结果策略

### 9.2 `FMetaplotTransition`

- `SourceNodeId`
- `TargetNodeId`
- `Conditions`

约束：

- 所有条件为 `AND`
- 支持前置完成检查、随机概率与自定义条件（自定义条件待落地）

### 9.3 流程共享状态（非插件内置）

插件不再提供 `DefaultBlackboard` 或实例级键值表。若需要跨节点共享数据，请在游戏模块中实现（例如 `GameInstance`、`Subsystem`、Actor 组件），并在 `UMetaplotStoryTask` 内通过构造函数注入、`Outer` 查找或接口回调访问。

### 9.4 `UMetaplotInstance`

职责包括：

- 管理活跃节点
- 跟踪任务运行状态
- 聚合节点执行结果
- 作为未来网络复制与 RPC 的承载对象

## 10. 编辑器设计

### 10.1 节点管理

- 创建：模板拖拽或右键新增
- 删除：删除节点并清理相关连线
- 属性编辑：名称、描述、类型、布局、任务、策略

### 10.2 连线管理

- 从右引脚拖到目标节点左引脚
- 建线时实时做规则校验
- 支持删除连线

### 10.3 前置条件与 Transition 条件编辑

**范围调整：** 专用的「前置条件编辑器」已从开发计划中移除，不再实现独立的条件列表面板或深度定制的条件编辑向导。

当前预期：

- `FMetaplotTransition` / `FMetaplotCondition` 仍保留在数据模型中，可通过引擎默认属性面板或现有 Details 暴露方式做**有限编辑**（以实际已接线的定制为准）。
- 复杂分支与条件表达改由游戏侧逻辑、`StoryTask` 与外部状态配合完成，而非依赖插件内专用条件 UI。

### 10.4 Details 设计方向

当前主路径：

- 直接编辑真实资产
- 通过 `DetailsContext` 锁定选中节点
- 通过 `DetailCustomization` 与 `IPropertyHandle` 组织 UI

不再作为主路径的旧方案：

- `UMetaplotNodeDetailsProxy` 作为节点编辑中间对象
- 独立手写 `NodeDetails` 面板承担核心编辑

### 10.5 撤销与刷新

目标是统一通过事务与刷新入口收敛：

- `Modify`
- Transaction
- `MarkPackageDirty`
- 统一 UI 刷新链路

当前事务链路已部分进入主路径，但图编辑层面仍未彻底完整。

## 11. Runtime 与蓝图集成

### 11.1 运行时入口

已存在最小蓝图入口，例如：

```cpp
UMetaplotInstance* StartMetaplotInstance(UMetaplotFlow* FlowAsset);
```

### 11.2 任务接口

```cpp
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class UMetaplotStoryTask : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent) void EnterTask(UMetaplotInstance* Instance, FGuid NodeId);
    UFUNCTION(BlueprintNativeEvent) EMetaplotTaskRunState TickTask(UMetaplotInstance* Instance, float DeltaTime);
    UFUNCTION(BlueprintNativeEvent) void ExitTask(UMetaplotInstance* Instance, FGuid NodeId);
};
```

### 11.3 子流程

**已移出开发计划：** 不再实现「在节点内启动子 `UMetaplotFlow`、结束后返回父流程」的运行时编排。若需要多段流程，可在游戏侧串联多个 `UMetaplotFlow` 实例或拆分资产，由项目代码调度。

## 12. 校验、调试与网络

### 12.1 校验

目标校验项包括：

- 循环依赖
- 孤立节点
- 多起始节点
- 无效行为引用
- 空条件

当前已实现的是建线过程中的基础实时校验；全量校验面板尚未完成。

### 12.2 调试

目标包括：

- 编辑器内模拟运行
- 游戏内调试命令
- 运行日志与节点/条件变化跟踪

当前这些仍主要停留在规划或骨架阶段。

### 12.3 多人网络

设计目标：

- 服务器权威推进流程
- 状态复制到客户端
- 通过 `Server RPC` 承载输入确认类推进
- 通过 `Multicast RPC` 承载纯表现效果同步

当前状态：

- 仍未进入完整实现阶段

## 13. 扩展性设计

### 13.1 任务发现与注册

目标是通过发现/注册机制收集可用 `UMetaplotStoryTask` 类，并在编辑器中供选择。当前未完成。

### 13.2 节点类型扩展

可通过任务接口与策略组合，模拟等待、分支、有限次循环等更高层语义。

### 13.3 双轨任务模型

当前为了兼容旧路径与推进同构化，任务编辑处于双轨模型：

- 新路径：`NodeData / InstanceData`
- 兼容路径：旧字段 fallback

后续方向：

- 继续向 `FStateTreeEditorNode` 等价深度收敛
- 评估更完整的 `FInstancedStruct` 驱动编辑模型

## 14. 近期路线图

### 14.1 推荐开发顺序

以下顺序已**不再包含**「前置条件专用 UI」「子流程运行时编排」等已移出计划的能力。建议优先补齐可持续编辑与验证闭环，再进入网络与多人相关能力：

1. 完整 Undo/Redo 与统一刷新入口
2. StoryTask 发现 / 注册机制
3. 编辑器内模拟运行
4. Details / StateTree 对齐收尾
5. 网络复制与多人实例策略

排序依据：

- 1~2 决定编辑器是否真正可用（任务与图编辑可持续迭代）
- 3 决定是否具备低成本验证闭环
- 4 用于收尾当前 Details 架构，避免长期半过渡状态
- 5 价值很高，但依赖前面编辑、调试链路先稳定

### 14.2 各项完成标准

#### 完整 Undo/Redo 与统一刷新入口

- 节点、连线、任务修改都进入统一事务链（条件字段若仍暴露在 Details 中，则与之一致）
- 不再依赖分散的 `ForceRefresh` 或局部补刷新
- Ctrl+Z / Ctrl+Y 对图表与 Details 表现一致

#### StoryTask 发现 / 注册机制

- 编辑器可稳定枚举可用 `UMetaplotStoryTask`
- Add Task 不依赖临时硬编码或脆弱扫描路径
- 新增 C++ / 蓝图任务后可被编辑器识别

#### 编辑器内模拟运行

- 可在不进入游戏世界的情况下手动步进或自动运行
- 活跃节点、节点结果与任务状态变化可见
- 可用于验证任务链、分支与失败路径

#### Details / StateTree 对齐收尾

- Transition 详情继续向资产直编路径收敛
- 双轨任务模型继续向更完整的编辑节点模型靠拢
- 体验细节收尾，减少当前 UI 与 StateTree 的整洁度差距

#### 网络复制与多人实例策略

- 明确 `UMetaplotInstance` 的复制承载形态
- 补齐 `Server RPC` / `Multicast RPC` 的主链路设计
- 明确全局共享流程与玩家私有流程的实例策略

## 15. 附录

### 15.1 术语

- Metaplot：事件流程编辑器与资产系统名称
- 节点：流程中的阶段点
- 连线：节点间有向边
- 流程共享状态：由游戏侧存储；插件不内置键值黑板
- StoryTask：节点内的具体任务单元

### 15.2 与蓝图编辑器的差异

- 蓝图面向通用逻辑编程
- Metaplot 更聚焦事件流程设计
- Metaplot 节点与连线语义更收敛，更强调阶段推进与阅读性

### 15.3 简化示例

```text
阶段0: Start
  -> 阶段1: 等待5秒
  -> 阶段2: 起风
  -> 阶段3: 下大雪
  -> 阶段4: End
```
