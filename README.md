# TestDevUI

## 项目说明

本项目主要用于开发 **[Metaplot](Plugins/Metaplot)** 插件的功能验证与示例工程。

---

## Metaplot 事件流程编辑器 — 设计文档

**设计文档版本：** 2.0（目标规格；实现进度以代码为准）  
**插件实现版本：** **0.1.0**（见 `Plugins/Metaplot/Metaplot.uplugin` 的 `VersionName`，当前标记为 Beta）  
**适用范围：** Unreal Engine 5.3+ 编辑器扩展插件  
**核心定位：** 模拟事物发展过程，支持**线性、非循环、单向**事件流，适用于多人游戏叙事与玩法流程设计。

### 当前实现状态标记（基于仓库代码）

标记说明：`[x] 已完成`、`[~] 部分完成`、`[ ] 未完成`。

#### A. 基础框架

- [x] 插件双模块结构（`Metaplot` Runtime + `MetaplotEditor` Editor）已建立并可加载。
- [x] `Metaplot` 资产类别、资产类型动作注册、资产工厂已实现。
- [x] 自定义资产模型已对齐 `UMetaplotFlow`（`Nodes` / `Transitions` / `DefaultBlackboard` / `StartNodeId`），旧 `UMetaplotScenarioAsset` 保留为兼容类（继承 `UMetaplotFlow`）。
- [~] 独立编辑器窗口骨架已实现（**控制板** / **流程图表** / Details 三区域）。流程图表已接入主视图画布（`SMetaplotFlowGraphWidget`：科技树风格网格、节点与圆角折线连线可视化、左右引脚拖拽建线、节点块拖拽换位、拖拽时可放置区域高亮（绿/红）、实时规则校验、中键平移与底部横向滚动条，并与控制板选中同步）。控制板已承载节点/连线操作与列表区，仍有进一步 UMG 风格化空间。

#### B. 编辑器目标功能（第 5 章）

- [~] 节点创建/删除/属性编辑已具备最小可用能力（控制板可增删，属性可在 Details 编辑）；主视图支持节点拖拽移动并带事务写回；复制/粘贴与完整图节点语义未实现。
- [~] 连线在资产与主视图中可读写（`Transitions` 数据 + 流程图表区圆角折线/箭头绘制）；画布支持左右引脚拖拽建线，并实时校验与拒绝无效连接（自连、右往左、同行跨列、重复、成环）；连线绘制已加入 lane 分流、长跨度走廊 bundling 与交互高亮。前置条件编辑器与更完整图编辑语义仍未实现。
- [ ] 前置条件编辑器未实现（暂无条件模型与对应 UI）。
- [ ] 自动对齐、拓扑布局、子流程折叠未实现。
- [ ] 行为蓝图下拉/新建行为蓝图/蓝图拖拽建点未实现。
- [ ] Undo/Redo 的节点连线事务级编辑未实现（仅新建资产带 `RF_Transactional`）。

#### C. 运行时目标功能（第 4/7/8 章）

- [ ] `UMetaplotSubsystem`、`UMetaplotInstance` 未实现。
- [ ] `FMetaplotNode`、`FMetaplotTransition`、`FMetaplotCondition`、黑板结构未实现。
- [ ] 行为接口 `IMetaplotBehavior` 与行为注册表未实现。
- [ ] 蓝图运行时 API（如 `StartMetaplotInstance`、`MetaplotAdvanceNode`）未实现。
- [ ] 子流程运行时编排能力未实现。

#### D. 校验/调试与多人网络（第 6/7 章）

- [~] 实时校验已部分实现（画布拖拽建线时：环、自连、方向、同行跨列、重复连线校验与拒绝）；孤立节点/无效行为等全量校验面板未实现。
- [ ] 编辑器内模拟运行模式未实现。
- [ ] 游戏内调试命令/日志体系未实现。
- [ ] 网络复制、Server/Multicast RPC、多人实例策略未实现。

> 备注：Runtime 与「模拟运行」等仍为规划能力；编辑器侧已具备 **流程图表可视化 + 引脚拖拽建线 + 节点拖拽换位（可放置高亮）+ 基础实时校验** 与基础资产编辑，整体仍偏「可编辑脚手架」而非完整第 5 章编辑器。

### 1. 引言

#### 1.1 项目背景

在多人游戏开发中，常常需要设计一系列按顺序发生、有条件推进的事件流程，例如：

- 天气变化序列（晴 → 起风 → 大雪）
- 关卡任务链（接任务 → 收集物品 → 交付 → 开门）
- 世界状态演变（和平 → 紧张 → 战争）

现有方案（蓝图、Level Sequence、Timeline）难以直观表达多阶段、带前置条件的单向流程，且对多人网络复制支持不佳。

#### 1.2 编辑器命名与定位

- **名称：** Metaplot（元剧情）
- **定位：** 专为 Unreal Engine 设计的事件流程编辑与运行时系统，以《文明6》科技树风格的视觉图表为核心，允许设计师快速创建、编辑、调试线性单向事件链。

#### 1.3 与 UE 内置蓝图编辑器的关系

- **不替代蓝图**，而是与蓝图协作：每个节点可绑定一个实现 `IMetaplotBehavior` 的蓝图类或 Actor，具体行为由蓝图 / C++ 实现。
- 作为独立编辑器窗口，复用 UE 的 Slate UI、资产管理、细节面板（Details）及事务系统（Undo/Redo）。

#### 1.4 核心设计原则

| 原则 | 说明 |
|------|------|
| 单向无环 | 事件流严格向前推进，不允许多次重复同一节点（无循环边） |
| 视觉清晰 | 类似科技树的网格化分层布局，降低复杂流程的理解成本 |
| 事件驱动 | 节点仅定义「何时可以进入」以及「进入后做什么」，由运行时解释器推进 |
| 多人就绪 | 服务器权威，状态自动复制到所有客户端，支持多个并行流程实例 |
| 高扩展性 | 节点行为通过接口实现，不同模块（天气、任务、物理等）可独立扩展 |

---

### 2. 整体架构

#### 2.1 插件组成

- **Editor Module：** 提供自定义资产类型 `UMetaplotFlow`、独立编辑器窗口（`FMetaplotEditor`）、图表绘制逻辑。
- **Runtime Module：** 轻量级运行时，包含 `UMetaplotSubsystem`（管理全局实例）、`UMetaplotInstance`（单个流程实例）、节点行为接口。

#### 2.2 自定义资产类型

`UMetaplotFlow`：继承自 `UObject`，存储节点数组、连线数组、黑板定义。可被多个游戏实例引用。

#### 2.3 编辑器窗口结构

目标结构示意如下；**当前工程**已规范为三区域：**控制板**（左列）、**流程图表**（中央）、**Details**（右列）。流程图表已具备网格、节点、圆角折线连线、左右引脚拖拽建线、中键平移与底部横向滚动条；尚未实现独立「节点模板库」侧栏与顶部工具栏条目。

```
+-------------------------------------------------+
| 菜单栏 (保存, 校验, 调试)                        |
+-------------------------------------------------+
| 工具栏 (节点模板库, 布局辅助, 模拟运行)          |
+---------------+---------------------------------+
| 节点模板库    | 图表视图 (Canvas)               |
| (可折叠)      |   - 网格背景                     |
|               |   - 节点 (事件点)                |
|               |   - 连线 (圆角折线箭头)          |
|               |   - 底部横向滚动条               |
+---------------+---------------------------------+
| 细节面板 (当前选中节点/连线的属性)               |
+-------------------------------------------------+
```

#### 2.4 运行时模块结构

- **服务器：** 运行 `UMetaplotInstance`，推进节点，广播状态。
- **客户端：** 接收复制状态，显示当前事件，不可直接修改流程。

---

### 3. 视觉与布局设计（受《文明6》科技树启发）

**实现进度（代码，简要）：** 流程图表区已按 `StageIndex` / `LayerIndex` 绘制网格与节点卡片（宽约 160px），连线为浅色圆角折线路径 + 箭头；左右引脚拖拽建线、实时校验（自连/右往左/同行跨列/重复/成环）已具备；节点块拖拽换位与可放置区域高亮（绿/红）已具备；连线路由已具备 lane 分流与跨多列 bundling，且支持悬停/选中关联高亮；中键平移与底部横向滚动条已具备。**尚未实现：** 运行时四态着色（§3.2）、悬停信息卡片（§3.2）、更完整图编辑语义（复制粘贴/高级批处理等）。

#### 3.1 网格化阶段 / 时间轴布局

- **水平轴（X）：** 表示推进顺序（阶段 0, 1, 2, …），即拓扑排序后的索引。
- **垂直轴（Y）：** 表示并行事件域（同一阶段可容纳多个独立并行节点，按 Y 轴分布）。
- 每个节点拥有网格坐标 `(Stage, Layer)`，编辑器提供自动布局功能。

#### 3.2 节点视觉样式

- **图标：** 根据节点类型显示不同图标（起始、普通、条件、并行、终止）。
- **状态色：**
  - **灰色（锁定）：** 前置条件未满足
  - **蓝色（可用）：** 前置满足，等待触发
  - **绿色（已完成）：** 已执行完毕
  - **橙色（进行中）：** 正在执行行为（例如大雪持续播放）
- **悬停卡片：** 显示节点 ID、描述、前置条件列表、关联的行为类名。
- **尺寸：** 固定宽度（约 160px），高度可变（根据描述文本自动）。

#### 3.3 连线样式

- 带箭头的圆角折线路径，颜色浅灰（科技树风格），并对同组连线做 lane 分流。
- 画布连线从**右引脚**拖到目标节点**左引脚**。
- 实时拒绝无效连接：自连、右往左、同行跨列、重复连线、成环。
- 跨多列连线使用走廊 bundling 路由，减少重叠与交叉观感。
- 连线支持交互高亮：与悬停/选中节点关联的连线更亮更粗，其余弱化显示。

#### 3.4 画布导航

- 鼠标中键拖拽平移。
- 底部横向滚动条移动（与画布水平位移同步）。

#### 3.5 与蓝图编辑器的视觉差异

| 维度 | Metaplot |
|------|----------|
| 更简洁 | 节点仅保留左右流程引脚（非蓝图多类型输入/输出引脚） |
| 强调流程 | 节点按网格排列，连线清晰 |

---

### 4. 核心数据模型

#### 4.1 节点结构 (`FMetaplotNode`)

- **标识：** `FGuid NodeId`
- **展示：** `FText NodeName`、`FText Description`（多行）
- **类型：** `EMetaplotNodeType`（Start, Normal, Conditional, Parallel, Terminal）
- **布局：** `StageIndex`（水平阶段）、`LayerIndex`（垂直层）
- **行为：** `BehaviorObject`（软引用，可指向实现 `IMetaplotBehavior` 的 `UObject`）、`BehaviorActorClass`（可选，需 Spawn Actor 时使用）

#### 4.2 连线结构 (`FMetaplotTransition`)

- `SourceNodeId` → `TargetNodeId`
- **过渡条件：** `Conditions` 数组，**所有条件必须同时满足（AND）**

其中 `FMetaplotCondition` 支持：

- 前置节点完成检查
- 黑板变量比较（整数、浮点、布尔）
- 随机概率（0~1）
- 行为自定义条件（通过 `IMetaplotBehavior::CanTransition`）

#### 4.3 黑板 (`FMetaplotBlackboardEntry`)

每条目包含：`FName Name`、`EMetaplotBlackboardType`（Bool, Int, Float, String, Object）及对应值字段。运行时黑板变量可网络复制（`UPROPERTY(Replicated)`）。

#### 4.4 流程资产 (`UMetaplotFlow`)

- `TArray<FMetaplotNode> Nodes`
- `TArray<FMetaplotTransition> Transitions`
- `TArray<FMetaplotBlackboardEntry> DefaultBlackboard`
- `FGuid StartNodeId`（起始节点 ID，唯一）

#### 4.5 流程运行时实例 (`UMetaplotInstance`)

- 在**服务器**上创建，拥有独立状态机。
- 存储当前节点集合（支持**并行节点**，允许多个节点同时处于「进行中」）。
- 维护黑板副本。
- **复制策略（设计目标）：**
  - 实例对象复制到所有客户端（`ReplicatedUsing` 等）。
  - 黑板通过 `GetLifetimeReplicatedProps` 标记。
  - 节点状态数组（`NodeId` → `EMetaplotNodeRuntimeState`）整体复制。
  - **RPC：** `ServerRequestAdvance(FGuid NodeId)`、`MulticastOnNodeStateChanged` 等。

---

### 5. 编辑器功能设计

#### 5.1 节点管理

- **创建：** 从模板库拖拽或右键「添加节点」。
- **删除：** 选中节点按 Delete 或右键删除（同时删除相关连线）。
- **复制/粘贴：** 复制节点及所有出边/入边（GUID 重新生成）。
- **属性：** 在细节面板中修改名称、描述、类型、行为引用。

#### 5.2 连线管理

- 从节点右侧圆点拖到另一节点左侧圆点创建连线。
- **实时规则校验：** 若将导致有向环、自连、右往左、同行跨列或重复连线，拒绝创建并提示。
- 支持批量选择多条连线后删除。

#### 5.3 前置条件编辑器

- 在细节面板以列表显示 `Conditions`。
- 每个条件可配置类型（前置节点、黑板比较、随机）。
- 黑板比较：变量名、操作符（`==`, `!=`, `>`, `<`, `>=`, `<=`）与常量值。
- 由于是线性单向，**不支持 OR 条件**（保持简单）。

#### 5.4 布局辅助

- **自动对齐：** 按 Stage / Layer 对齐到网格。
- **拓扑排序布局：** 根据依赖关系自动计算 Stage 与 Layer（一键整理）。
- **折叠子流程：** 若节点行为引用子 `UMetaplotFlow`，可折叠为复合节点。

#### 5.5 与蓝图交互

- 细节面板行为下拉显示所有实现 `IMetaplotBehavior` 的蓝图类。
- 「新建行为蓝图」可生成继承接口的蓝图并打开编辑器。
- 可从蓝图编辑器拖拽函数到 Metaplot 图表，快速创建节点并绑定行为。

#### 5.6 撤销/重做

使用 UE `FTransaction` 系统，节点/连线操作均支持撤销。

---

### 6. 校验与调试

#### 6.1 实时校验

编辑器后台持续检查，错误显示在「错误列表」窗口：

| 校验项 | 检测方法 |
|--------|----------|
| 循环依赖 | 深度优先搜索，检测后向边 |
| 孤立节点 | 从起始节点出发不可达 |
| 多起始节点 | 允许但警告 |
| 无效行为引用 | `BehaviorObject` 指向的类未实现接口 |
| 空条件 | 条件列表包含无效条目 |

#### 6.2 模拟运行模式

在编辑器窗口内模拟，不依赖游戏世界：手动步进或自动运行，高亮当前活跃节点；黑板变量可实时查看与手动修改。

#### 6.3 游戏内调试

- 控制台：`Metaplot.Debug <FlowAssetPath>` 显示运行时流程状态。
- 日志：节点进入/退出、黑板变量变化。

---

### 7. 导出与运行时集成（多人游戏支持）

#### 7.1 运行时数据

- `UMetaplotFlow` 资产可直接用于运行时，无需强制导出。
- 可选：导出为二进制缓存以加快加载。

#### 7.2 蓝图 API（示例）

```cpp
// 启动流程实例（服务器调用）
UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
UMetaplotInstance* StartMetaplotInstance(UObject* WorldContext, UMetaplotFlow* FlowAsset, AActor* Owner);

// 在行为中调用：当前节点完成，流程可继续
UFUNCTION(BlueprintCallable, Server, Reliable)
void MetaplotAdvanceNode(UMetaplotInstance* Instance, FGuid NodeId);

// 查询黑板
UFUNCTION(BlueprintPure)
int32 GetMetaplotBlackboardInt(UMetaplotInstance* Instance, FName Key);
```

#### 7.3 网络复制细节

- `UMetaplotInstance` 可挂载为 `UActorComponent` 或独立 `AActor`，按场景选择。
- 建议挂在 **GameState** 或专用 **MetaplotManager** Actor 上，保证服务器权威与全客户端可见。
- 需在所有客户端表现的效果（如爆炸），行为内使用 **Multicast RPC**。
- **大量玩家各自独立流程（如个人任务）：** 为每名玩家创建独立 `UMetaplotInstance`，存于 **`APlayerState`**；节点行为涉及玩家输入时，使用 **Server RPC** 请求推进，避免客户端作弊。
- **全局共享流程：** 可将 `UMetaplotInstance` 作为 **`AGameState` 子组件**，由引擎复制机制同步到各客户端。

---

### 8. 扩展性设计

#### 8.1 行为接口 (`IMetaplotBehavior`)

```cpp
UINTERFACE(BlueprintType)
class UMetaplotBehavior : public UInterface { GENERATED_BODY() };

class IMetaplotBehavior
{
public:
    // 进入节点时调用（服务器）
    virtual void OnEnterNode(UMetaplotInstance* Instance, const FMetaplotNode& Node) = 0;
    // 可选：每帧（持续行为，如下雪）
    virtual void OnTickNode(UMetaplotInstance* Instance, float DeltaTime) {}
    // 除常规条件外的自定义过渡判断
    virtual bool CanTransition(UMetaplotInstance* Instance, const FMetaplotNode& Node, const FMetaplotTransition& Transition) const { return true; }
    virtual void OnExitNode(UMetaplotInstance* Instance, const FMetaplotNode& Node) {}
    virtual FString GetDebugString(const FMetaplotNode& Node) const { return Node.NodeName.ToString(); }
};
```

可为 C++ 类或蓝图类（继承 `UMetaplotBehaviorObject` 辅助基类，或直接实现接口）。若使用 `BehaviorActorClass`，运行时可 Spawn 临时 Actor 作为行为载体。

#### 8.2 注册自定义行为

通过 `FMetaplotBehaviorRegistry` 单例注册行为模板，出现在编辑器下拉菜单中。

#### 8.3 节点类型扩展

内置类型有限时，可通过不同行为接口组合模拟「等待」「选择分支」「有限次循环」等。

#### 8.4 子流程支持

行为对象可持有 `UMetaplotFlow` 引用，在 `OnEnterNode` 中启动子流程；子流程结束后调用 `MetaplotAdvanceNode` 回到父流程。

---

### 9. 性能与约束

| 项目 | 指标 |
|------|------|
| 最大节点数（编辑器） | ≥500 节点时操作流畅（60fps 目标） |
| 最大节点数（运行时） | 视内存与带宽，建议单流程 ≤200 节点 |
| 运行时推进 | 每节点进入时 O(1) 检查出边条件，O(E) 广播状态（设计目标） |
| 网络复制 | 黑板变量按需复制，节点状态增量同步（设计目标） |

**约束：**

- 严格禁止循环依赖，编辑器强制校验。
- 不支持节点内任意跳转，**仅能通过连线推进**。
- 同一流程实例中可有多处「进行中」节点（并行），但须处于**同一阶段或更后阶段**。

---

### 10. 附录

#### 附录 A — 术语表

| 术语 | 解释 |
|------|------|
| Metaplot | 元剧情；本编辑器名称，也指事件流程资产 |
| 节点 | 流程中的一个阶段点，表示可执行的事件阶段 |
| 连线 | 节点间有向边，表示推进方向与条件 |
| 黑板 | 流程实例的全局变量存储，可网络复制 |
| 行为对象 | 实现 `IMetaplotBehavior` 的 `UObject`/`Actor`，定义节点具体逻辑 |

#### 附录 B — 与蓝图编辑器的功能对比

| 功能 | 蓝图编辑器 | Metaplot 编辑器 |
|------|------------|-----------------|
| 节点类型 | 函数、事件、变量等 | 流程事件节点 |
| 连线逻辑 | 执行流、数据流 | 仅条件推进流 |
| 网络复制 | 需手动实现 | 内置支持（设计目标） |
| 布局风格 | 自由布局 | 网格化阶段布局 |
| 适用场景 | 通用逻辑编程 | 事件流程设计 |

#### 附录 C — 简单示例：天气演变流程

```
阶段0: 起始节点 "Start" (行为: 黑板 Weather = "Sunny")
   ↓
阶段1: 节点 "等待5秒" (行为: Delay 5s)
   ↓
阶段2: 节点 "起风" (行为: Play Wind Effect)
   ↓ (条件: 黑板 WindCount < 3)
阶段2: 节点 "再次起风" (并行, 增加 WindCount)
   ↓ (条件: WindCount >= 3)
阶段3: 节点 "下大雪" (行为: Spawn Snow Particles)
   ↓
阶段4: 终止节点 "End"
```

#### 附录 D — 多人游戏部署建议

- **服务器权威：** 仅在服务器创建与推进 `UMetaplotInstance`。
- **复制：** 状态通过组件/Actor 复制同步到客户端。
- **输入与作弊：** 需要玩家确认或输入的节点，由客户端发起 **Server RPC**，服务器校验后再 `MetaplotAdvanceNode`。
- **表现同步：** 纯表现类效果使用 **Multicast** 或复制属性驱动。

---

## 仓库中的插件

- 路径：`Plugins/Metaplot`
- 模块：`Metaplot`（Runtime）、`MetaplotEditor`（Editor）
- 版本：`VersionName` **0.1.0**，`IsBetaVersion` **true**（以 `Metaplot.uplugin` 为准）

详见插件内源码与 `Metaplot.uplugin` 元数据。
