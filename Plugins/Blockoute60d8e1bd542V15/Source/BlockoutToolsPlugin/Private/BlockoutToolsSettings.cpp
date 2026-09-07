// Copyright 2024 Dmitry Karpukhin. All Rights Reserved.

#include "BlockoutToolsSettings.h"

UBlockoutToolsSettings::UBlockoutToolsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)

{ 
	BlockoutMaterialType = BlockoutMaterialType_Grid;
	BlockoutMaterialColor = FLinearColor(1.0f, 0.3f, 0.05f, 1.0f);
	bBlockoutMaterialUseGrid = true;
	bBlockoutWorldAligned = false;
	BlockoutMaterialGridSize = 100.0f;
	BlockoutMaterialCheckerLuminance = 0.3f;
	BlockoutMaterialRoughness = 0.3f;
	bBlockoutMaterialUseTopColor = true;
	BlockoutMaterialTopColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);
//	BlockoutCustomMaterial = ConstructorStatics.BlockoutCustomMaterial.Get();
}