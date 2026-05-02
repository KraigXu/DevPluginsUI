// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTaskBase.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryLinker.h"
#include "MetaStoryPropertyRef.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryDelegate.h"
#include "AutoRTFM.h"
#include <atomic>

#include "MetaStoryTestTypes.generated.h"

class UMetaStory;
struct FMetaStoryInstanceData;

// Test log that can be passed as external data.
USTRUCT()
struct FMetaStoryTestLog
{
	GENERATED_BODY()

	struct FLogItem
	{
		FLogItem() = default;
		FLogItem(const FName& InName, const FString& InMessage) : Name(InName), Message(InMessage) {}
		FName Name;
		FString Message; 
	};
	TArray<FLogItem> Items;
	
	void Log(const FName& Name, const FString& Message)
	{
		Items.Emplace(Name, Message);
	}
};

struct FTestStateTreeExecutionContext : public FMetaStoryExecutionContext
{
	FTestStateTreeExecutionContext(UObject& InOwner, const UMetaStory& InStateTree, FMetaStoryInstanceData& InInstanceData, const FOnCollectStateTreeExternalData& CollectExternalDataCallback = {}, const EMetaStoryRecordTransitions RecordTransitions = EMetaStoryRecordTransitions::No)
		: FMetaStoryExecutionContext(InOwner, InStateTree, InInstanceData, CollectExternalDataCallback, RecordTransitions)
	{
		// Handle getting pointer to the test log.
		FMetaStoryDataView TestLogView(TBaseStructure<FMetaStoryTestLog>::Get(), (uint8*)&Log);
		
		CollectExternalDataDelegate = FOnCollectStateTreeExternalData::CreateLambda(
			[TestLogView](const FMetaStoryExecutionContext& Context, const UMetaStory* MetaStory, TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs, TArrayView<FMetaStoryDataView> OutDataViews)
			{
				for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
				{
					const FMetaStoryExternalDataDesc& ItemDesc = ExternalDataDescs[Index];
					if (ItemDesc.Struct == TBaseStructure<FMetaStoryTestLog>::Get())
					{
						OutDataViews[Index] = TestLogView;
						break;
					}
				}
				return true;
			});
	}

	FMetaStoryTestLog Log;

	void LogClear()
	{
		Log.Items.Empty();
	}

	struct FLogOrder
	{
		FLogOrder(const FTestStateTreeExecutionContext& InContext, const int32 InIndex)
			: Context(&InContext), Index(InIndex)
		{}

		FLogOrder Then(const FName& Name) const
		{
			int32 NextIndex = Index;
			while (NextIndex < Context->Log.Items.Num())
			{
				const FMetaStoryTestLog::FLogItem& Item = Context->Log.Items[NextIndex];
				if (Item.Name == Name)
				{
					break;
				}
				NextIndex++;
			}
			return FLogOrder(*Context, NextIndex);
		}

		FLogOrder Then(const FName& Name, const FStringView Message) const
		{
			int32 NextIndex = Index;
			while (NextIndex < Context->Log.Items.Num())
			{
				const FMetaStoryTestLog::FLogItem& Item = Context->Log.Items[NextIndex];
				if (Item.Name == Name && Item.Message == Message)
				{
					break;
				}
				NextIndex++;
			}
			return FLogOrder(*Context, NextIndex);
		}

		operator bool() const
		{
			return Index < Context->Log.Items.Num();
		}

	private:
		const FTestStateTreeExecutionContext* Context;
		int32 Index = 0;
	};

	FLogOrder Expect(const FName& Name) const
	{
		return FLogOrder(*this, 0).Then(Name);
	}

	FLogOrder Expect(const FName& Name, const FStringView Message) const
	{
		return FLogOrder(*this, 0).Then(Name, Message);
	}

	template <class ...Args>
	bool ExpectInActiveStates(const Args&... States)
	{
		FName ExpectedStateNames[] = { States... };
		const int32 NumExpectedStateNames = sizeof...(States);

		TArray<FName> ActiveStateNames = GetActiveStateNames();

		if (ActiveStateNames.Num() != NumExpectedStateNames)
		{
			return false;
		}

		for (int32 Index = 0; Index != NumExpectedStateNames; Index++)
		{
			if (ExpectedStateNames[Index] != ActiveStateNames[Index])
			{
				return false;
			}
		}

		return true;
	}
	
};

USTRUCT()
struct FTestEval_AInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float FloatA = 0.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 IntA = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bBoolA = false;
};

USTRUCT()
struct FTestEval_A : public FMetaStoryEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestEval_AInstanceData;

	FTestEval_A() = default;
	FTestEval_A(const FName InName) { Name = InName; }
	virtual ~FTestEval_A() override {}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
};

USTRUCT()
struct FTestEval_Custom : public FMetaStoryEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestEval_AInstanceData;

	FTestEval_Custom() = default;
	FTestEval_Custom(const FName InName) { Name = InName; }

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void TreeStart(FMetaStoryExecutionContext& Context) const override
	{
		if (CustomEnterStateFunc)
		{
			CustomEnterStateFunc(Context, this);
		}
	}

	virtual void TreeStop(FMetaStoryExecutionContext& Context) const override 
	{
		if (CustomExitStateFunc)
		{
			CustomExitStateFunc(Context, this);
		}
	}

	virtual void Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		if (CustomTickFunc)
		{
			CustomTickFunc(Context, this);
		}
	}

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}

	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;

	TFunction<void(FMetaStoryExecutionContext&, const FTestEval_Custom*)> CustomEnterStateFunc;
	TFunction<void(FMetaStoryExecutionContext&, const FTestEval_Custom*)> CustomExitStateFunc;
	TFunction<void(FMetaStoryExecutionContext&, const FTestEval_Custom*)> CustomTickFunc;
};

USTRUCT()
struct FTestTask_BInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	float FloatB = 0.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 IntB = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bBoolB = false;
};

USTRUCT()
struct FTestTask_B : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_BInstanceData;

	FTestTask_B() = default;
	FTestTask_B(const FName InName) { Name = InName; }
	virtual ~FTestTask_B() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		TestLog.Log(Name,  TEXT("EnterState"));
		return EMetaStoryRunStatus::Running;
	}

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}
	
	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
};

USTRUCT()
struct FTestTask_PrintValueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Value = 0;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<int32> ArrayValue;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EMetaStoryRunStatus EnterStateRunStatus = EMetaStoryRunStatus::Running;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EMetaStoryRunStatus TickRunStatus = EMetaStoryRunStatus::Running;

	FString GetArrayDescription() const
	{
		if (ArrayValue.IsEmpty())
		{
			return TEXT("{}");
		}
		TStringBuilder<256> StringBuilder;
		StringBuilder << TEXT('{');
		bool bFirst = true;
		for (int32 It : ArrayValue)
		{
			if (!bFirst)
			{
				StringBuilder << TEXT(", ");
			}
			bFirst = false;
			StringBuilder << It;
		}
		StringBuilder << TEXT('}');
		return StringBuilder.ToString();
	}
};

USTRUCT()
struct FTestTask_PrintValueExecutionRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Value = 0;
};

USTRUCT()
struct FTestTask_PrintValue : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PrintValueInstanceData;
	using FExecutionRuntimeDataType = FTestTask_PrintValueExecutionRuntimeData;

	FTestTask_PrintValue() = default;
	FTestTask_PrintValue(const FName InName) { Name = InName; }
	virtual ~FTestTask_PrintValue() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual const UStruct* GetExecutionRuntimeDataType() const override { return FExecutionRuntimeDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestLog.Log(Name, FString::Printf(TEXT("EnterState%d"), InstanceData.Value));
		TestLog.Log(Name, FString::Printf(TEXT("EnterState:%s"), *InstanceData.GetArrayDescription()));
		TestLog.Log(Name, FString::Printf(TEXT("EnterState=%s"), *StaticEnum<EMetaStoryStateChangeType>()->GetNameStringByValue((int64)Transition.ChangeType)));

		const FExecutionRuntimeDataType& ExecData = Context.GetExecutionRuntimeData(*this);
		TestLog.Log(Name, FString::Printf(TEXT("EnterStateExec=%d"), ExecData.Value));
		if (CustomEnterStateFunc)
		{
			CustomEnterStateFunc(Context, this);
		}
		return InstanceData.EnterStateRunStatus;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestLog.Log(Name, FString::Printf(TEXT("ExitState%d"), InstanceData.Value));
		TestLog.Log(Name, FString::Printf(TEXT("ExitState:%s"), *InstanceData.GetArrayDescription()));
		TestLog.Log(Name, FString::Printf(TEXT("ExitState=%s"), *StaticEnum<EMetaStoryStateChangeType>()->GetNameStringByValue((int64)Transition.ChangeType)));
		if (CustomExitStateFunc)
		{
			CustomExitStateFunc(Context, this);
		}
	}

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestLog.Log(Name, FString::Printf(TEXT("Tick%d"), InstanceData.Value));
		TestLog.Log(Name, FString::Printf(TEXT("Tick:%s"), *InstanceData.GetArrayDescription()));
		if (CustomTickFunc)
		{
			CustomTickFunc(Context, this);
		}
		return InstanceData.TickRunStatus;
	}

	virtual void TriggerTransitions(FMetaStoryExecutionContext& Context) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestLog.Log(Name, FString::Printf(TEXT("TriggerTransitions%d"), InstanceData.Value));
		TestLog.Log(Name, FString::Printf(TEXT("TriggerTransitions:%s"), *InstanceData.GetArrayDescription()));
	}

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}
	
	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
	TFunction<void(FMetaStoryExecutionContext&, const FTestTask_PrintValue*)> CustomEnterStateFunc;
	TFunction<void(FMetaStoryExecutionContext&, const FTestTask_PrintValue*)> CustomExitStateFunc;
	TFunction<void(FMetaStoryExecutionContext&, const FTestTask_PrintValue*)> CustomTickFunc;
};

USTRUCT()
struct FTestTask_PrintAndResetValue : public FTestTask_PrintValue
{
	GENERATED_BODY()

	using FTestTask_PrintValue::FTestTask_PrintValue;

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		EMetaStoryRunStatus Status = Super::EnterState(Context, Transition);
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		InstanceData.Value = ResetValue;
		InstanceData.ArrayValue = ResetArrayValue;
		return Status;
	}

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		EMetaStoryRunStatus Status = Super::Tick(Context, DeltaTime);
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		InstanceData.Value = ResetValue;
		InstanceData.ArrayValue = ResetArrayValue;
		return Status;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		Super::ExitState(Context, Transition);
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		InstanceData.Value = ResetValue;
		InstanceData.ArrayValue = ResetArrayValue;
	}

	UPROPERTY()
	int32 ResetValue = 0;

	UPROPERTY()
	TArray<int32> ResetArrayValue;
};

USTRUCT()
struct FTestTask_PrintValue_TransitionTick : public FTestTask_PrintValue
{
	GENERATED_BODY()

	FTestTask_PrintValue_TransitionTick()
	{
		bShouldCallTick = true;
		bShouldAffectTransitions = true;
	}
	FTestTask_PrintValue_TransitionTick(const FName InName)
		: FTestTask_PrintValue(InName)
	{
		bShouldCallTick = true;
		bShouldAffectTransitions = true;
	}
};

USTRUCT()
struct FTestTask_PrintValue_TransitionNoTick : public FTestTask_PrintValue
{
	GENERATED_BODY()

	FTestTask_PrintValue_TransitionNoTick()
	{
		bShouldCallTick = false;
		bShouldAffectTransitions = true;
	}
	FTestTask_PrintValue_TransitionNoTick(const FName InName)
		: FTestTask_PrintValue(InName)
	{
		bShouldCallTick = false;
		bShouldAffectTransitions = true;
	}
};

USTRUCT()
struct FTestTask_StopTreeInstanceData
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTask_StopTree : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PrintValueInstanceData;

	FTestTask_StopTree() = default;
	explicit FTestTask_StopTree(const FName InName) { Name = InName; }
	virtual ~FTestTask_StopTree() override {}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (Phase == EMetaStoryUpdatePhase::EnterStates)
		{
			return Context.Stop();
		}
		return EMetaStoryRunStatus::Running;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (Phase == EMetaStoryUpdatePhase::ExitStates)
		{
			Context.Stop();
		}
	}

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		if (Phase == EMetaStoryUpdatePhase::TickStateTree)
		{
			return Context.Stop();
		}
		return EMetaStoryRunStatus::Running;
	};

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}
	
	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
	
	/** Indicates in which phase the call to Stop should be performed. Possible values are EnterStates, ExitStats and TickStateTree */
	UPROPERTY()
	EMetaStoryUpdatePhase Phase = EMetaStoryUpdatePhase::Unset;
};

USTRUCT()
struct FTestTask_StandInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Value = 0;
	
	UPROPERTY()
	int32 CurrentTick = 0;
};

USTRUCT()
struct FTestTask_Stand : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_StandInstanceData;
	
	FTestTask_Stand() = default;
	FTestTask_Stand(const FName InName) { Name = InName; }
	virtual ~FTestTask_Stand() {}

	virtual const UStruct* GetInstanceDataType() const { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		TestLog.Log(Name, TEXT("EnterState"));
		TestLog.Log(Name, FString::Printf(TEXT("EnterState=%s"), *StaticEnum<EMetaStoryStateChangeType>()->GetNameStringByValue((int64)Transition.ChangeType)));

		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		
		if (Transition.ChangeType == EMetaStoryStateChangeType::Changed)
		{
			InstanceData.CurrentTick = 0;
		}
		return EnterStateResult;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);

		if (Transition.CurrentRunStatus == EMetaStoryRunStatus::Succeeded)
		{
			TestLog.Log(Name, TEXT("ExitSucceeded"));
		}
		else if (Transition.CurrentRunStatus == EMetaStoryRunStatus::Failed)
		{
			TestLog.Log(Name, TEXT("ExitFailed"));
		}
		else if (Transition.CurrentRunStatus == EMetaStoryRunStatus::Stopped)
		{
			TestLog.Log(Name, TEXT("ExitStopped"));
		}

		TestLog.Log(Name, TEXT("ExitState"));
		TestLog.Log(Name, FString::Printf(TEXT("ExitState=%s"), *StaticEnum<EMetaStoryStateChangeType>()->GetNameStringByValue((int64)Transition.ChangeType)));
	}

	virtual void StateCompleted(FMetaStoryExecutionContext& Context, const EMetaStoryRunStatus CompletionStatus, const FMetaStoryActiveStates& CompletedActiveStates) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		TestLog.Log(Name, TEXT("StateCompleted"));
	}
	
	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		TestLog.Log(Name, TEXT("Tick"));

		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		
		InstanceData.CurrentTick++;
		
		return (InstanceData.CurrentTick >= TicksToCompletion) ? TickCompletionResult : EMetaStoryRunStatus::Running;
	};

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}
	
	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 TicksToCompletion = 1;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMetaStoryRunStatus TickCompletionResult = EMetaStoryRunStatus::Succeeded;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMetaStoryRunStatus EnterStateResult = EMetaStoryRunStatus::Running;
};

USTRUCT()
struct FTestTask_StandNoTick : public FTestTask_Stand
{
	GENERATED_BODY()

	FTestTask_StandNoTick()
	{
		bShouldCallTick = false;
	}

	FTestTask_StandNoTick(const FName InName)
		: FTestTask_Stand(InName)
	{
		bShouldCallTick = false;
	}
};


USTRUCT()
struct FTestTask_IntegersOutput_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Output)
	int32 IntA = 0;

	UPROPERTY(EditAnywhere, Category=Output)
	int32 IntB = 0;

};

USTRUCT()
struct FTestTask_IntegersOutput : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_IntegersOutput_InstanceData;

	FTestTask_IntegersOutput() = default;

	explicit FTestTask_IntegersOutput(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (CustomEnterStateFunc)
		{
			CustomEnterStateFunc(Context, *this);
		}

		return EMetaStoryRunStatus::Running;
	}

	TFunction<void(FMetaStoryExecutionContext&, const FTestTask_IntegersOutput&)> CustomEnterStateFunc;
};

USTRUCT()
struct FTestTask_PropertyRefOnNodeAndInstance_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (RefType = "int32"))
	FMetaStoryPropertyRef RefOnInstance;
};

USTRUCT()
struct FTestTask_PropertyRefOnNodeAndInstance : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PropertyRefOnNodeAndInstance_InstanceData;

	FTestTask_PropertyRefOnNodeAndInstance() = default;
	FTestTask_PropertyRefOnNodeAndInstance(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (CustomEnterStateFunc)
		{
			CustomEnterStateFunc(Context, *this);
		}

		return EMetaStoryRunStatus::Running;
	}

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (RefType = "int32"))
	FMetaStoryPropertyRef RefOnNode;

	TFunction<void(FMetaStoryExecutionContext&, const FTestTask_PropertyRefOnNodeAndInstance&)> CustomEnterStateFunc;
};

USTRUCT()
struct FMetaStoryTestConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameters)
	int32 Count = 1;

	static std::atomic<int32> GlobalCounter;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryTestConditionInstanceData);

USTRUCT(meta = (Hidden))
struct FMetaStoryTestCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryTestConditionInstanceData;

	FMetaStoryTestCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override
	{
		std::atomic<int32>& GlobalCounter = Context.GetInstanceData(*this).GlobalCounter;
		UE_AUTORTFM_OPEN
		{
			++GlobalCounter;
		};
		UE_AUTORTFM_ONABORT(&GlobalCounter)
		{
			--GlobalCounter;
		};
		return bTestConditionResult;
	}

	UPROPERTY()
	bool bTestConditionResult = true;
};

struct FMetaStoryTestRunContext
{
	int32 Count = 0;
};


USTRUCT()
struct FMetaStoryTest_PropertyStructA
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	int32 A = 0;
};

USTRUCT()
struct FMetaStoryTest_PropertyStructB
{
	GENERATED_BODY()

	FMetaStoryTest_PropertyStructB()
	{
		NumConstructed++;
	}
	~FMetaStoryTest_PropertyStructB()
	{
		NumConstructed--;
	}

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;

	static inline int32 NumConstructed = 0;
};

USTRUCT()
struct FMetaStoryTest_PropertyStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	int32 A = 0;

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;

	UPROPERTY(EditAnywhere, Category = "")
	FMetaStoryTest_PropertyStructB StructB;
};

UCLASS(HideDropdown)
class UMetaStoryTest_PropertyObjectInstanced : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "")
	int32 A = 0;

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedStruct InstancedStruct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FGameplayTag> ArrayOfTags;
};

UCLASS(HideDropdown)
class UMetaStoryTest_PropertyObjectInstancedWithB : public UMetaStoryTest_PropertyObjectInstanced
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "")
	int32 B = 0;
};

UCLASS(HideDropdown)
class UMetaStoryTest_PropertyObject : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, Instanced, Category = "")
	TObjectPtr<UMetaStoryTest_PropertyObjectInstanced> InstancedObject;

	UPROPERTY(EditAnywhere, Instanced, Category = "")
	TArray<TObjectPtr<UMetaStoryTest_PropertyObjectInstanced>> ArrayOfInstancedObjects;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<int32> ArrayOfInts;

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedStruct InstancedStruct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FInstancedStruct> ArrayOfInstancedStructs;

	UPROPERTY(EditAnywhere, Category = "")
	FMetaStoryTest_PropertyStruct Struct;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FMetaStoryTest_PropertyStruct> ArrayOfStruct;
};

UCLASS(HideDropdown)
class UMetaStoryTest_PropertyObject2 : public UObject
{
	GENERATED_BODY()
public:
};

USTRUCT()
struct FMetaStoryTest_PropertyCopy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	FMetaStoryTest_PropertyStruct Item;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FMetaStoryTest_PropertyStruct> Array;

	UPROPERTY(EditAnywhere, Category = "", EditFixedSize)
	TArray<FMetaStoryTest_PropertyStruct> FixedArray;

	UPROPERTY(EditAnywhere, Category = "")
	FMetaStoryTest_PropertyStruct CArray[3];
};

USTRUCT()
struct FMetaStoryTest_PropertyRefSourceStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	FMetaStoryTest_PropertyStruct Item;

	UPROPERTY(EditAnywhere, Category = "Output")
	FMetaStoryTest_PropertyStruct OutputItem;

	UPROPERTY(EditAnywhere, Category = "")
	TArray<FMetaStoryTest_PropertyStruct> Array;
};

USTRUCT()
struct FMetaStoryTest_PropertyRefTargetStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "/Script/MetaStoryTestSuite.MetaStoryTest_PropertyStruct"))
	FMetaStoryPropertyRef RefToStruct;

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "Int32"))
	FMetaStoryPropertyRef RefToInt;

	UPROPERTY(EditAnywhere, Category = "", meta = (RefType = "/Script/MetaStoryTestSuite.MetaStoryTest_PropertyStruct", IsRefToArray))
	FMetaStoryPropertyRef RefToStructArray;
};

USTRUCT()
struct FMetaStoryTest_PropertyCopyObjects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	TObjectPtr<UObject> Object;

	UPROPERTY(EditAnywhere, Category = "")
	TSubclassOf<UObject> Class;

	UPROPERTY(EditAnywhere, Category = "")
	TSoftObjectPtr<UObject> SoftObject;

	UPROPERTY(EditAnywhere, Category = "")
	TSoftClassPtr<UObject> SoftClass;
};

USTRUCT()
struct FTestPropertyFunction_InstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Input = 0;

	UPROPERTY()
	int32 Result = 0;
};
METASTORY_POD_INSTANCEDATA(FTestPropertyFunction_InstanceData);

USTRUCT()
struct FTestPropertyFunction : public FMetaStoryPropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestPropertyFunction_InstanceData;

	FTestPropertyFunction() = default;
	FTestPropertyFunction(const FName InName) { Name = InName; }
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FMetaStoryExecutionContext& Context) const override
	{
		FTestPropertyFunction_InstanceData& InstanceData = Context.GetInstanceData<FTestPropertyFunction_InstanceData>(*this);
		InstanceData.Result = InstanceData.Input + 1;
	}
};

USTRUCT()
struct FTestTask_PrintValue_StructRef_NoBindingUpdateInstanceData : public FTestTask_PrintValueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FMetaStoryTest_PropertyStruct PropertyStruct;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/MetaStoryTestSuite.MetaStoryTest_PropertyStruct"))
	FMetaStoryStructRef StructRef;
};

USTRUCT()
struct FTestTask_PrintValue_StructRef_NoBindingUpdate : public FTestTask_PrintValue
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_PrintValue_StructRef_NoBindingUpdateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	FTestTask_PrintValue_StructRef_NoBindingUpdate()
	{
		bShouldCopyBoundPropertiesOnTick = false;
	}
	FTestTask_PrintValue_StructRef_NoBindingUpdate(const FName InName)
		: FTestTask_PrintValue(InName)
	{
		bShouldCopyBoundPropertiesOnTick = false;
	}
};

USTRUCT()
struct FMetaStoryTestBooleanConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameters)
	bool bSuccess = true;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryTestBooleanConditionInstanceData);

USTRUCT(meta = (Hidden))
struct FMetaStoryTestBooleanCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryTestBooleanConditionInstanceData;

	FMetaStoryTestBooleanCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override
	{
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		TestLog.Log(Name,  FString::Printf(TEXT("TestCondition=%d"), InstanceData.bSuccess));
		return InstanceData.bSuccess;
	}

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}

	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
};

USTRUCT()
struct FTestTask_BroadcastDelegate_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher OnEnterDelegate;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher OnTickDelegate;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher OnExitDelegate;
};

USTRUCT()
struct FTestTask_BroadcastDelegate : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	FTestTask_BroadcastDelegate() = default;
	FTestTask_BroadcastDelegate(const FName InName) { Name = InName; }

	using FInstanceDataType = FTestTask_BroadcastDelegate_InstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_BroadcastDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_BroadcastDelegate_InstanceData>(*this);
		Context.BroadcastDelegate(InstanceData.OnEnterDelegate);
		return EMetaStoryRunStatus::Running;
	}

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		FTestTask_BroadcastDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_BroadcastDelegate_InstanceData>(*this);
		Context.BroadcastDelegate(InstanceData.OnTickDelegate);
		return EMetaStoryRunStatus::Running;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_BroadcastDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_BroadcastDelegate_InstanceData>(*this);
		Context.BroadcastDelegate(InstanceData.OnExitDelegate);
	}
};

USTRUCT()
struct FTestTask_ListenDelegate_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateListener Listener;

	uint32 TriggersAmount = 0u;
};

USTRUCT()
struct FTestTask_ListenDelegate : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	FTestTask_ListenDelegate() = default;
	FTestTask_ListenDelegate(const FName InName) { Name = InName; }

	using FInstanceDataType = FTestTask_ListenDelegate_InstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_ListenDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_ListenDelegate_InstanceData>(*this);
		FMetaStoryTestLog& TestLog = Context.GetExternalData(LogHandle);
		Context.BindDelegate(InstanceData.Listener, FSimpleDelegate::CreateLambda([&TestLog, this, InstanceDataRef = Context.GetInstanceDataStructRef(*this)]()
			{
				if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
				{
					++InstanceData->TriggersAmount;
					TestLog.Log(Name, FString::Printf(TEXT("OnDelegate%d"), InstanceData->TriggersAmount));
				}
			}));

		return EMetaStoryRunStatus::Running;
	}

	virtual bool Link(FMetaStoryLinker& Linker) override
	{
		Linker.LinkExternalData(LogHandle);
		return true;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (bRemoveOnExit)
		{
			FTestTask_ListenDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_ListenDelegate_InstanceData>(*this);
			Context.UnbindDelegate(InstanceData.Listener);
		}
	}
	
	bool bRemoveOnExit = true;
	TStateTreeExternalDataHandle<FMetaStoryTestLog> LogHandle;
};

USTRUCT()
struct FTestTask_RebroadcastDelegate_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateListener Listener;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher Dispatcher;
};

USTRUCT()
struct FTestTask_RebroadcastDelegate : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	FTestTask_RebroadcastDelegate() = default;
	FTestTask_RebroadcastDelegate(const FName InName) { Name = InName; }

	using FInstanceDataType = FTestTask_RebroadcastDelegate_InstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_RebroadcastDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_RebroadcastDelegate_InstanceData>(*this);

		Context.BindDelegate(InstanceData.Listener, FSimpleDelegate::CreateLambda([Dispatcher = InstanceData.Dispatcher, WeakContext = Context.MakeWeakExecutionContext()]()
			{
				WeakContext.BroadcastDelegate(Dispatcher);
			}));

		return EMetaStoryRunStatus::Running;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_RebroadcastDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_RebroadcastDelegate_InstanceData>(*this);
		Context.UnbindDelegate(InstanceData.Listener);
	}
};

USTRUCT()
struct FTestTask_CustomFuncOnDelegate_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateListener Listener;
};

USTRUCT()
struct FTestTask_CustomFuncOnDelegate : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	FTestTask_CustomFuncOnDelegate() = default;
	FTestTask_CustomFuncOnDelegate(const FName InName) { Name = InName; }

	using FInstanceDataType = FTestTask_CustomFuncOnDelegate_InstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		FTestTask_CustomFuncOnDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_CustomFuncOnDelegate_InstanceData>(*this);

		Context.BindDelegate(InstanceData.Listener, FSimpleDelegate::CreateLambda([Func = CustomFunc, Listener = InstanceData.Listener, WeakContext = Context.MakeWeakExecutionContext()]()
			{
				Func(WeakContext, Listener);
			}));
		return EMetaStoryRunStatus::Running;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (bRemoveOnExit)
		{
			FTestTask_CustomFuncOnDelegate_InstanceData& InstanceData = Context.GetInstanceData<FTestTask_CustomFuncOnDelegate_InstanceData>(*this);
			Context.UnbindDelegate(InstanceData.Listener);
		}
	}

	TFunction<void(const FMetaStoryWeakExecutionContext&, FMetaStoryDelegateListener)> CustomFunc;

	bool bRemoveOnExit = true;
};

USTRUCT()
struct FTestTask_DispatcherOnNodeAndInstance_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher DispatcherOnInstance;
};

USTRUCT()
struct FTestTask_DispatcherOnNodeAndInstance : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_DispatcherOnNodeAndInstance_InstanceData;

	FTestTask_DispatcherOnNodeAndInstance() = default;

	explicit FTestTask_DispatcherOnNodeAndInstance(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		const FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

		Context.BroadcastDelegate(DispatcherOnNode);
		Context.BroadcastDelegate(InstanceData.DispatcherOnInstance);

		return EMetaStoryRunStatus::Running;
	}

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateDispatcher DispatcherOnNode;
};

USTRUCT()
struct FTestTask_ListenerOnNode_InstanceData
{
	GENERATED_BODY()
};

USTRUCT()
struct FTestTask_ListenerOnNode : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_ListenerOnNode_InstanceData;

	FTestTask_ListenerOnNode() = default;

	explicit FTestTask_ListenerOnNode(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct();  }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		Context.BindDelegate(ListenerOnNode, FSimpleDelegate::CreateLambda(
			[this, WeakExecContext = Context.MakeWeakExecutionContext()]()
			{
				CustomFunc(WeakExecContext);
			}));

		return EMetaStoryRunStatus::Running;
	}

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateListener ListenerOnNode;

	TFunction<void(const FMetaStoryWeakExecutionContext&)> CustomFunc;
};

USTRUCT()
struct FTestTask_ListenerOnInstance_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMetaStoryDelegateListener ListenerOnInstance;
};

USTRUCT()
struct FTestTask_ListenerOnInstance : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_ListenerOnInstance_InstanceData;

	FTestTask_ListenerOnInstance() = default;

	explicit FTestTask_ListenerOnInstance(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		const FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
		Context.BindDelegate(InstanceData.ListenerOnInstance, FSimpleDelegate::CreateLambda(
			[this, WeakExecContext = Context.MakeWeakExecutionContext()]()
			{
				CustomFunc(WeakExecContext);
			}));

		return EMetaStoryRunStatus::Running;
	}

	TFunction<void(const FMetaStoryWeakExecutionContext&)> CustomFunc;
};

// @todo: add tests for CArray type
USTRUCT()
struct FIntWrapper
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Default)
	int32 Value = 0;


	bool operator==(const FIntWrapper& Other) const = default;
};

USTRUCT()
struct FTestTask_OutputBindingsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FIntWrapper InputIntWrapper;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FIntWrapper> InputIntWrapperArray;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 InputIntA = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 InputIntB = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	uint8 OutputBool : 1 = false;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 OutputInt = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	FIntWrapper OutputIntWrapper;

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FIntWrapper> OutputIntWrapperArray;
};

USTRUCT()
struct FTestTask_OutputBindingsTask : public FMetaStoryTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTestTask_OutputBindingsTaskInstanceData;

	FTestTask_OutputBindingsTask() = default;

	explicit FTestTask_OutputBindingsTask(FName InName)
	{
		Name = InName;
	}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (CustomEnterFunc.IsSet())
		{
			CustomEnterFunc(Context.GetInstanceData(*this));
		}

		return bShouldFailEnter ? EMetaStoryRunStatus::Failed : EMetaStoryRunStatus::Running;
	}

	virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override
	{
		if (CustomTickFunc.IsSet())
		{
			CustomTickFunc(Context.GetInstanceData(*this));
		}

		return bShouldFailTick ? EMetaStoryRunStatus::Failed : EMetaStoryRunStatus::Running;
	}

	virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override
	{
		if (CustomExitFunc.IsSet())
		{
			CustomExitFunc(Context.GetInstanceData(*this));
		}
	}

	TFunction<void(FTestTask_OutputBindingsTaskInstanceData&)> CustomEnterFunc;
	TFunction<void(FTestTask_OutputBindingsTaskInstanceData&)> CustomTickFunc;
	TFunction<void(FTestTask_OutputBindingsTaskInstanceData&)> CustomExitFunc;

	UPROPERTY()
	uint8 bShouldFailEnter : 1 = false;

	UPROPERTY()
	uint8 bShouldFailTick : 1 = false;
};
