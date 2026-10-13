#include "Items/InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::AddItem(const FInventoryItem& Item)
{
	Items.Add(Item);
}

bool UInventoryComponent::RemoveItem(FName ItemId)
{
	const int32 Index = Items.IndexOfByPredicate([ItemId](const FInventoryItem& It) { return It.ItemId == ItemId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Items.RemoveAt(Index);
	return true;
}

bool UInventoryComponent::HasItem(FName ItemId) const
{
	return Items.ContainsByPredicate([ItemId](const FInventoryItem& It) { return It.ItemId == ItemId; });
}

float UInventoryComponent::GetTotalWeight() const
{
	float Total = BaseGearWeight;
	for (const FInventoryItem& It : Items)
	{
		Total += It.Weight;
	}
	return Total;
}

float UInventoryComponent::GetSpeedMultiplier() const
{
	if (!bWeightAffectsSpeed)
	{
		return 1.f;
	}
	return FMath::GetMappedRangeValueClamped(
		FVector2f(ComfortableWeight, MaxWeight),
		FVector2f(1.f, MinSpeedMultiplier),
		GetTotalWeight());
}
