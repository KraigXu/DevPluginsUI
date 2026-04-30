# TestDevUI

## 项目说明

本项目主要用于开发与验证 **[Metaplot](Plugins/Metaplot)** 插件，并作为其示例工程。

## 当前状态

- 插件版本：`0.1.0 Beta`
- 适用范围：Unreal Engine `5.3+`
- 当前阶段：编辑器与 Runtime 已具备最小可用骨架，整体仍处于持续完善阶段
- README 全量目标完成度：约 `31% ~ 35%`
- Details 对齐 StateTree 专项完成度：约 `90%`

## 主要文档

- [Metaplot 主设计文档](Docs/MetaplotDesign.md)
  - 包含设计规格、当前实现状态与“近期路线图”
- [Metaplot 开发流程文档](Docs/MetaplotDevelopmentFlow.md)
  - 包含日常开发流程、模块职责、功能落地步骤与验证清单

## 仓库中的插件

- 路径：`Plugins/Metaplot`
- 模块：`Metaplot`（Runtime）、`MetaplotEditor`（Editor）
- 版本：`VersionName` `0.1.0`，`IsBetaVersion` `true`

## 当前重点缺口

与 [Metaplot 主设计文档](Docs/MetaplotDesign.md) 对齐，**仍在推进**的方向主要包括：

- 节点/连线事务级 **Undo/Redo** 与统一刷新、更完整的图编辑语义
- **StoryTask** 注册/发现机制
- 编辑器内模拟运行与游戏内调试体系
- 网络复制、Server/Multicast RPC 与多人实例策略

**已从开发计划移除**（不再作为交付目标）：前置条件专用编辑器；自动对齐、拓扑布局、子流程折叠；行为蓝图下拉/新建蓝图/蓝图拖拽建点；子流程运行时编排。说明见主文档 §5、§10.3、§11.3。

详细设计、当前实现状态、Details 对齐结论与路线图请见 [Metaplot 主设计文档](Docs/MetaplotDesign.md)。
