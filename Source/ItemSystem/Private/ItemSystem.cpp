// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemSystem.h"
#include "Core/ItemSystemLog.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FItemSystemModule"

DEFINE_LOG_CATEGORY(LogItemSystem);

static TAutoConsoleVariable<int32> CVarItemSystemQA(
	TEXT("ItemSystem.QA"),
	0,
	TEXT("Enable verbose QA logging for ItemSystem (0/1)."),
	ECVF_Default
);

bool IsItemSystemQAEnabled()
{
	return CVarItemSystemQA.GetValueOnGameThread() > 0;
}

void FItemSystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FItemSystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FItemSystemModule, ItemSystem)
