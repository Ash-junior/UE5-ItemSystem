#include "Debug/ItemCheatManager.h"
#include "Core/ItemSystemManager.h"
#include "Core/InventoryComponent.h"
#include "Data/ItemDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

namespace
{
	enum class ETokenType
	{
		Tag,
		And,
		Or,
		Not,
		LParen,
		RParen,
		End
	};

	struct FToken
	{
		ETokenType Type = ETokenType::End;
		FString Text;
	};

	class FTagQueryParser
	{
	public:
		explicit FTagQueryParser(const FString& InText)
			: Text(InText)
		{
			Tokenize();
		}

		bool Build(FGameplayTagQuery& OutQuery, FString& OutError)
		{
			FGameplayTagQueryExpression Root = ParseExpression();
			if (bError)
			{
				OutError = Error;
				return false;
			}

			if (Current().Type != ETokenType::End)
			{
				OutError = TEXT("Unexpected tokens at end of query.");
				return false;
			}

			OutQuery = FGameplayTagQuery::BuildQuery(Root, Text);
			if (OutQuery.IsEmpty())
			{
				OutError = TEXT("Query is empty.");
				return false;
			}

			return true;
		}

	private:
		FString Text;
		TArray<FToken> Tokens;
		int32 Index = 0;
		bool bError = false;
		FString Error;

	private:
		void Tokenize()
		{
			const int32 Len = Text.Len();
			int32 i = 0;

			while (i < Len)
			{
				const TCHAR C = Text[i];
				if (FChar::IsWhitespace(C))
				{
					++i;
					continue;
				}

				if (C == '(')
				{
					Tokens.Add({ETokenType::LParen, TEXT("(")});
					++i;
					continue;
				}
				if (C == ')')
				{
					Tokens.Add({ETokenType::RParen, TEXT(")")});
					++i;
					continue;
				}

				// Read a word (tag or operator)
				int32 Start = i;
				while (i < Len)
				{
					const TCHAR WC = Text[i];
					if (FChar::IsWhitespace(WC) || WC == '(' || WC == ')')
					{
						break;
					}
					++i;
				}

				const FString Word = Text.Mid(Start, i - Start);
				const FString Upper = Word.ToUpper();
				if (Upper == TEXT("AND"))
				{
					Tokens.Add({ETokenType::And, Word});
				}
				else if (Upper == TEXT("OR"))
				{
					Tokens.Add({ETokenType::Or, Word});
				}
				else if (Upper == TEXT("NOT"))
				{
					Tokens.Add({ETokenType::Not, Word});
				}
				else
				{
					Tokens.Add({ETokenType::Tag, Word});
				}
			}

			Tokens.Add({ETokenType::End, TEXT("")});
		}

		const FToken& Current() const
		{
			return Tokens[Index];
		}

		const FToken& Consume()
		{
			return Tokens[Index++];
		}

		bool Match(ETokenType Type)
		{
			if (Current().Type == Type)
			{
				Consume();
				return true;
			}
			return false;
		}

		FGameplayTagQueryExpression ParseExpression()
		{
			return ParseOr();
		}

		FGameplayTagQueryExpression ParseOr()
		{
			FGameplayTagQueryExpression Left = ParseAnd();
			while (Match(ETokenType::Or))
			{
				FGameplayTagQueryExpression Right = ParseAnd();
				FGameplayTagQueryExpression OrExpr;
				OrExpr.AnyExprMatch()
					.AddExpr(Left)
					.AddExpr(Right);
				Left = OrExpr;
			}
			return Left;
		}

		FGameplayTagQueryExpression ParseAnd()
		{
			FGameplayTagQueryExpression Left = ParseUnary();
			while (Match(ETokenType::And))
			{
				FGameplayTagQueryExpression Right = ParseUnary();
				FGameplayTagQueryExpression AndExpr;
				AndExpr.AllExprMatch()
					.AddExpr(Left)
					.AddExpr(Right);
				Left = AndExpr;
			}
			return Left;
		}

		FGameplayTagQueryExpression ParseUnary()
		{
			if (Match(ETokenType::Not))
			{
				FGameplayTagQueryExpression Inner = ParseUnary();
				FGameplayTagQueryExpression NotExpr;
				NotExpr.NoExprMatch().AddExpr(Inner);
				return NotExpr;
			}

			return ParsePrimary();
		}

		FGameplayTagQueryExpression ParsePrimary()
		{
			if (Match(ETokenType::LParen))
			{
				FGameplayTagQueryExpression Inner = ParseExpression();
				if (!Match(ETokenType::RParen))
				{
					bError = true;
					Error = TEXT("Missing closing parenthesis.");
				}
				return Inner;
			}

			if (Current().Type == ETokenType::Tag)
			{
				const FString TagText = Consume().Text;
				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagText), false);
				if (!Tag.IsValid())
				{
					bError = true;
					Error = FString::Printf(TEXT("Invalid tag: %s"), *TagText);
					return FGameplayTagQueryExpression();
				}

				FGameplayTagQueryExpression TagExpr;
				TagExpr.AllTagsMatch().AddTag(Tag);
				return TagExpr;
			}

			bError = true;
			Error = TEXT("Unexpected token.");
			return FGameplayTagQueryExpression();
		}
	};
}

void UItemCheatManager::Cheat_GiveItem(FString TagQueryString)
{
    // 1. Get the Player Pawn
    APawn* MyPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
    if (!MyPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: No Pawn found!"));
        return;
    }

    // 2. Find Inventory Component
    UInventoryComponent* Inventory = MyPawn->FindComponentByClass<UInventoryComponent>();
    if (!Inventory)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: Pawn has no Inventory Component!"));
        return;
    }

    // 3. Find System Manager
    UItemSystemManager* Manager = UItemSystemManager::Get(this);
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cheat: ItemSystemManager is missing from GameState!"));
        return;
    }

    // 4. Parse a tag query from the string
    // Supports: AND / OR / NOT with parentheses. Example: "Item.Type.Offense AND NOT Item.Rarity.Legendary"
    FGameplayTagQuery Query;
    FString ParseError;
    FTagQueryParser Parser(TagQueryString);
    if (!Parser.Build(Query, ParseError))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: Invalid Tag Query. %s"), *ParseError);
        return;
    }

    // 5. Get Item and Grant it
    UItemDefinition* FoundItem = Manager->GetItemByQuery(Query);
    if (FoundItem)
    {
        Inventory->Server_GrantItem(FoundItem, 1);
        UE_LOG(LogTemp, Log, TEXT("Cheat: Granted item %s"), *FoundItem->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: No item found matching query: %s"), *TagQueryString);
    }
}

void UItemCheatManager::Cheat_ClearInventory()
{
    // Implementation left as an exercise (set CurrentItem to nullptr in Inventory)
    // For now, simple log.
    UE_LOG(LogTemp, Log, TEXT("Cheat: Clear Inventory not fully implemented yet."));
}

