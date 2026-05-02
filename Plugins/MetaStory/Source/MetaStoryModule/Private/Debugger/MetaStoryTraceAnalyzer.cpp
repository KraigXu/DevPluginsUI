// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryTraceAnalyzer.h"
#include "Debugger/MetaStoryDebugger.h"
#include "Debugger/MetaStoryTraceProvider.h"
#include "Debugger/MetaStoryTraceTypes.h"
#include "Serialization/MemoryReader.h"
#include "TraceServices/Model/AnalysisSession.h"

FMetaStoryTraceAnalyzer::FMetaStoryTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FMetaStoryTraceProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FMetaStoryTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_AssetDebugId, "MetaStoryDebugger", "AssetDebugIdEvent");
	Builder.RouteEvent(RouteId_WorldTimestamp, "MetaStoryDebugger", "WorldTimestampEvent");
	Builder.RouteEvent(RouteId_Instance, "MetaStoryDebugger", "InstanceEvent");
	Builder.RouteEvent(RouteId_InstanceFrame, "MetaStoryDebugger", "InstanceFrameEvent");
	Builder.RouteEvent(RouteId_Phase, "MetaStoryDebugger", "PhaseEvent");
	Builder.RouteEvent(RouteId_LogMessage, "MetaStoryDebugger", "LogEvent");
	Builder.RouteEvent(RouteId_State, "MetaStoryDebugger", "StateEvent");
	Builder.RouteEvent(RouteId_Task, "MetaStoryDebugger", "TaskEvent");
	Builder.RouteEvent(RouteId_Evaluator, "MetaStoryDebugger", "EvaluatorEvent");
	Builder.RouteEvent(RouteId_Transition, "MetaStoryDebugger", "TransitionEvent");
	Builder.RouteEvent(RouteId_Condition, "MetaStoryDebugger", "ConditionEvent");
	Builder.RouteEvent(RouteId_ActiveStates, "MetaStoryDebugger", "ActiveStatesEvent");
}

bool FMetaStoryTraceAnalyzer::OnEvent(const uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FMetaStoryAnalyzer"));

	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_WorldTimestamp:
		{
			WorldTime = EventData.GetValue<double>("WorldTime");
			break;
		}
	case RouteId_AssetDebugId:
		{
			FString ObjectName, ObjectPathName;
			EventData.GetString("TreeName", ObjectName);
			EventData.GetString("TreePath", ObjectPathName);

			TWeakObjectPtr<const UMetaStory> WeakMetaStory;
			{
				// This might not work when using a debugger on a client but should be fine in Editor as long as
				// we are not trying to find the object during GC. We might not currently be in the game thread.  
				// @todo STDBG: eventually errors should be reported in the UI
				FGCScopeGuard Guard;
				WeakMetaStory = FindObject<UMetaStory>(nullptr, *ObjectPathName);
			}

			if (const UMetaStory* MetaStory = WeakMetaStory.Get())
			{
				const uint32 CompiledDataHash = EventData.GetValue<uint32>("CompiledDataHash");
				if (MetaStory->LastCompiledEditorDataHash == CompiledDataHash)
				{
					Provider.AppendAssetDebugId(MetaStory, FMetaStoryIndex16(EventData.GetValue<uint16>("AssetDebugId")));
				}
				else
				{
					UE_LOG(LogMetaStory, Warning, TEXT("Traces are not using the same MetaStory asset version as the current asset."));
				}
			}
			else
			{
				UE_LOG(LogMetaStory, Warning, TEXT("Unable to find MetaStory asset: %s : %s"), *ObjectPathName, *ObjectName);
			}
			break;
		}
	case RouteId_Instance:
		{
			FString InstanceName;
			EventData.GetString("InstanceName", InstanceName);

			Provider.AppendInstanceEvent(FMetaStoryIndex16(EventData.GetValue<uint16>("AssetDebugId")),
				FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				*InstanceName,
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				WorldTime,
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"));
			break;
		}
	case RouteId_InstanceFrame:
		{
			TWeakObjectPtr<const UMetaStory> WeakMetaStory;
			if (Provider.GetAssetFromDebugId(FMetaStoryIndex16(EventData.GetValue<uint16>("AssetDebugId")), WeakMetaStory))
			{
				const FMetaStoryTraceInstanceFrameEvent Event(WorldTime, EMetaStoryTraceEventType::Push, WeakMetaStory.Get());
			
				Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
					Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
					FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceInstanceFrameEvent>(), Event));
			}
			else
			{
				UE_LOG(LogMetaStory, Error, TEXT("Instance frame event refers to an asset Id that wasn't added previously."));
			}

			break;
		}
	case RouteId_Phase:
		{
			const FMetaStoryTracePhaseEvent Event(WorldTime,
				EventData.GetValue<EMetaStoryUpdatePhase>("Phase"),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"),
				FMetaStoryStateHandle(EventData.GetValue<uint16>("StateIndex")));

			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTracePhaseEvent>(), Event));
			break;
		}
	case RouteId_LogMessage:
		{
			FString Message;
        	EventData.GetString("Message", Message);
			const FMetaStoryTraceLogEvent Event(WorldTime, EventData.GetValue<ELogVerbosity::Type>("Verbosity"), Message);

			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceLogEvent>(), Event));
			break;
		}
	case RouteId_State:
		{
			const FMetaStoryTraceStateEvent Event(WorldTime,
				FMetaStoryIndex16(EventData.GetValue<uint16>("StateIndex")),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"));

			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceStateEvent>(), Event));
			break;
		}
	case RouteId_Task:
		{
			FString TypePath, DataAsText, DebugText;
			FMemoryReaderView Archive(EventData.GetArrayView<uint8>("DataView"));
			Archive << TypePath;
			Archive << DataAsText;
			Archive << DebugText;

			const FMetaStoryTraceTaskEvent Event(WorldTime,
				FMetaStoryIndex16(EventData.GetValue<uint16>("NodeIndex")),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"),
				EventData.GetValue<EMetaStoryRunStatus>("Status"),
				TypePath, DataAsText, DebugText);

			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceTaskEvent>(), Event));
			break;
		}
	case RouteId_Evaluator:
		{
			FString TypePath, DataAsText, DebugText;
			FMemoryReaderView Archive(EventData.GetArrayView<uint8>("DataView"));
			Archive << TypePath;
			Archive << DataAsText;
			Archive << DebugText;

			const FMetaStoryTraceEvaluatorEvent Event(WorldTime,
				FMetaStoryIndex16(EventData.GetValue<uint16>("NodeIndex")),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"),
				TypePath, DataAsText, DebugText);

			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceEvaluatorEvent>(), Event));
			break;
		}
	case RouteId_Condition:
		{
			FString TypePath, DataAsText, DebugText;
			FMemoryReaderView Archive(EventData.GetArrayView<uint8>("DataView"));
			Archive << TypePath;
			Archive << DataAsText;
			Archive << DebugText;

			const FMetaStoryTraceConditionEvent Event(WorldTime,
				FMetaStoryIndex16(EventData.GetValue<uint16>("NodeIndex")),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"),
				TypePath, DataAsText, DebugText);
			
			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceConditionEvent>(), Event));
			break;
		}
	case RouteId_Transition:
		{
			const FMetaStoryTraceTransitionEvent Event(WorldTime,
				FMetaStoryTransitionSource(
					nullptr, // we simply reuse the struct to hold the received data, we don't need to provide a MetaStory
					EventData.GetValue<EMetaStoryTransitionSourceType>("SourceType"),
					FMetaStoryIndex16(EventData.GetValue<uint16>("TransitionIndex")),
					FMetaStoryStateHandle(EventData.GetValue<uint16>("TargetStateIndex")),
					EventData.GetValue<EMetaStoryTransitionPriority>("Priority")
					),
				EventData.GetValue<EMetaStoryTraceEventType>("EventType"));
			
			Provider.AppendEvent(FMetaStoryInstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial")),
				Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
				FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceTransitionEvent>(), Event));
			break;
		}
	case RouteId_ActiveStates:
		{
			FMetaStoryTraceActiveStatesEvent Event(WorldTime);

			TArray<FMetaStoryStateHandle> ActiveStates(EventData.GetArrayView<uint16>("ActiveStates"));
			TArray<uint16> AssetDebugIds(EventData.GetArrayView<uint16>("AssetDebugIds"));

			if (ensureMsgf(ActiveStates.Num() == AssetDebugIds.Num(), TEXT("Each state is expected to have a matching asset id")))
			{
				FMetaStoryInstanceDebugId InstanceDebugId(EventData.GetValue<uint32>("InstanceId"), EventData.GetValue<uint32>("InstanceSerial"));
				TWeakObjectPtr<const UMetaStory> WeakMetaStory;

				// If empty, we create the event with an empty list of states for the main MetaStory.
				if (ActiveStates.IsEmpty())
				{
					if (Provider.GetAssetFromInstanceId(InstanceDebugId, WeakMetaStory))
					{
						FMetaStoryTraceActiveStates::FAssetActiveStates& NewPair = Event.ActiveStates.PerAssetStates.Emplace_GetRef();
						NewPair.WeakMetaStory = WeakMetaStory;
					}
				}
				else
				{
					FMetaStoryIndex16 LastAssetDebugId;
					FMetaStoryTraceActiveStates::FAssetActiveStates* AssetActiveStates = nullptr;
					for (int Index = 0; Index < ActiveStates.Num(); ++Index)
					{
						FMetaStoryIndex16 AssetDebugId(AssetDebugIds[Index]);
						if (AssetDebugId != LastAssetDebugId)
						{
							if (Provider.GetAssetFromDebugId(AssetDebugId, WeakMetaStory))
							{
								FMetaStoryTraceActiveStates::FAssetActiveStates& NewPair = Event.ActiveStates.PerAssetStates.Emplace_GetRef();
								NewPair.WeakMetaStory = WeakMetaStory;
								AssetActiveStates = &NewPair;
								LastAssetDebugId = AssetDebugId;
							}
							else
							{
								UE_LOG(LogMetaStory, Error, TEXT("Instance frame event refers to an asset Id that wasn't added previously."));
								continue;
							}
						}

						check(AssetActiveStates != nullptr);
						AssetActiveStates->ActiveStates.Push(ActiveStates[Index]);
					}
				}

				Provider.AppendEvent(InstanceDebugId,
					Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Cycle")),
					FMetaStoryTraceEventVariantType(TInPlaceType<FMetaStoryTraceActiveStatesEvent>(), Event));
			}
			break;
		}
	default:
		ensureMsgf(false, TEXT("Unhandle route id: %u"), RouteId);
	}

	return true;
}

#endif // WITH_METASTORY_TRACE_DEBUGGER
