// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/TokenizedMessage.h"
#include "MetaStoryPropertyBindings.h"
#include "MetaStoryCompilerLog.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStoryState;

/** MetaStory compiler log message */
USTRUCT()
struct FMetaStoryCompilerLogMessage
{
	GENERATED_BODY()

	FMetaStoryCompilerLogMessage() = default;
	FMetaStoryCompilerLogMessage(const EMessageSeverity::Type InSeverity, const UMetaStoryState* InState, const FMetaStoryBindableStructDesc& InItem, const FString& InMessage)
		: Severity(InSeverity)
		, State(InState)
		, Item(InItem)
		, Message(InMessage)
	{
	}

	/** Severity of the message. */
	UPROPERTY()
	int32 Severity = EMessageSeverity::Error;

	/** (optional) The MetaStory state the message refers to. */
	UPROPERTY()
	TObjectPtr<const UMetaStoryState> State = nullptr;

	/** (optional) The State tee item (condition/evaluator/task) the message refers to. */
	UPROPERTY()
	FMetaStoryBindableStructDesc Item;

	/** The message */
	UPROPERTY()
	FString Message;
};

/** Message log for MetaStory compilation */
USTRUCT()
struct FMetaStoryCompilerLog
{
	GENERATED_BODY()

	/** Pushes State to be reported along with the message. */
	void PushState(const UMetaStoryState* InState)
	{
		StateStack.Add(InState);
	}
	
	/** Pops State to be reported along with the message. @see FMetaStoryCompilerLogStateScope */
	void PopState(const UMetaStoryState* InState)
	{
		// Check for potentially miss matching push/pop
		check(StateStack.Num() > 0 && StateStack.Last() == InState);
		StateStack.Pop();
	}

	/** Returns current state context. */
	const UMetaStoryState* CurrentState() const { return StateStack.Num() > 0 ? StateStack.Last() : nullptr;  }

	/**
	 * Reports a message.
	 * @param InSeverity Severity of the message.
	 * @param InItem MetaStory item (condition/evaluator/task) the message affects.
	 * @param InMessage Message to display.
	 */
	void Report(EMessageSeverity::Type InSeverity, const FMetaStoryBindableStructDesc& InItem, const FString& InMessage)
	{
		Messages.Emplace(InSeverity, CurrentState(), InItem, InMessage);
	}

	/** Formatted version of the Report(). */
	template <typename... Types>
	void Reportf(EMessageSeverity::Type InSeverity,
						 const FMetaStoryBindableStructDesc& InItem,
						 UE::Core::TCheckedFormatString<FString::FmtCharType, Types...> Fmt,
						 Types... Args)
	{
		Report(InSeverity, InItem, FString::Printf(Fmt, Args...));
	}

	/** Formatted version of the Report(), omits Item for convenience. */
	template <typename... Types>
	void Reportf(EMessageSeverity::Type InSeverity, UE::Core::TCheckedFormatString<FString::FmtCharType, Types...> Fmt, Types... Args)
	{
		Report(InSeverity, FMetaStoryBindableStructDesc(), FString::Printf(Fmt, Args...));
	}

	/** Generates tokenized messages from the messages in this log */
	UE_API TArray<TSharedRef<FTokenizedMessage>> ToTokenizedMessages() const;

	/** Appends MetaStory log to log listing. */
	UE_API void AppendToLog(class IMessageLogListing* LogListing) const;

	/** Dumps MetaStory log to log */
	UE_API void DumpToLog(const FLogCategoryBase& Category) const;
	
protected:
	UPROPERTY()
	TArray<TObjectPtr<const UMetaStoryState>> StateStack;
	
	UPROPERTY()
	TArray<FMetaStoryCompilerLogMessage> Messages;
};

/** Helper struct to manage reported state within a scope. */
struct FMetaStoryCompilerLogStateScope
{
	FMetaStoryCompilerLogStateScope(const UMetaStoryState* InState, FMetaStoryCompilerLog& InLog)
		: Log(InLog)
		, State(InState)
	{
		Log.PushState(State);
	}

	~FMetaStoryCompilerLogStateScope()
	{
		Log.PopState(State);
	}

	FMetaStoryCompilerLog& Log; 
	const UMetaStoryState* State = nullptr;
};

#undef UE_API
