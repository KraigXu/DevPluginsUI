// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryCompilerLog.h"
#include "IMessageLogListing.h"
#include "Misc/UObjectToken.h"
#include "MetaStoryState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryCompilerLog)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TArray<TSharedRef<FTokenizedMessage>> FMetaStoryCompilerLog::ToTokenizedMessages() const
{
	TArray<TSharedRef<FTokenizedMessage>> TokenisedMessages;
	for (const FMetaStoryCompilerLogMessage& MetaStoryMessage : Messages)
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create((EMessageSeverity::Type)MetaStoryMessage.Severity);

		if (MetaStoryMessage.State != nullptr)
		{
			TSharedRef<FUObjectToken> ObjectToken = FUObjectToken::Create(MetaStoryMessage.State, FText::FromName(MetaStoryMessage.State->Name));

			// Add a dummy activation since default behavior opens content browser.
			static auto DummyActivation = [](const TSharedRef<class IMessageToken>&) {};
			ObjectToken->OnMessageTokenActivated(FOnMessageTokenActivated::CreateLambda(DummyActivation));
			
			Message->AddToken(ObjectToken);
		}

		if (MetaStoryMessage.Item.ID.IsValid())
		{
			Message->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LogMessageItem", " {0} '{1}': "),
				UEnum::GetDisplayValueAsText(MetaStoryMessage.Item.DataSource), FText::FromName(MetaStoryMessage.Item.Name))));
		}

		if (!MetaStoryMessage.Message.IsEmpty())
		{
			Message->AddToken(FTextToken::Create(FText::FromString(MetaStoryMessage.Message)));
		}

		TokenisedMessages.Add(Message);
	}

	return TokenisedMessages;
}

void FMetaStoryCompilerLog::AppendToLog(IMessageLogListing* LogListing) const
{
	LogListing->AddMessages(ToTokenizedMessages());
}

void FMetaStoryCompilerLog::DumpToLog(const FLogCategoryBase& Category) const
{
	for (const FMetaStoryCompilerLogMessage& MetaStoryMessage : Messages)
	{
		FString Message;
		
		if (MetaStoryMessage.State != nullptr)
		{
			Message += FString::Printf(TEXT("State '%s': "), *MetaStoryMessage.State->Name.ToString());
		}

		if (MetaStoryMessage.Item.ID.IsValid())
		{
			Message += FString::Printf(TEXT("%s '%s': "), *UEnum::GetDisplayValueAsText(MetaStoryMessage.Item.DataSource).ToString(), *MetaStoryMessage.Item.Name.ToString());
		}

		Message += MetaStoryMessage.Message;

		switch (MetaStoryMessage.Severity)
		{
		case EMessageSeverity::Error:
			UE_LOG_REF(Category, Error, TEXT("%s"), *Message);
			break;
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
			UE_LOG_REF(Category, Warning, TEXT("%s"), *Message);
			break;
		case EMessageSeverity::Info:
		default:
			UE_LOG_REF(Category, Log, TEXT("%s"), *Message);
			break;
		};
	}
}


#undef LOCTEXT_NAMESPACE

