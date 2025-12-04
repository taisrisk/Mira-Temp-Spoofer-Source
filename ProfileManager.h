#pragma once

#include <ntddk.h>
#include "Driver.h"

// Profile management functions
NTSTATUS BackupOriginalValues(VOID);
NTSTATUS ApplyRegistryModifications(_In_ PSYSTEM_PROFILE profile);
NTSTATUS RestoreRegistryValues(VOID);
NTSTATUS GenerateSystemProfile(_In_ PSEED_DATA seed, _Out_ PSYSTEM_PROFILE profile);

// Enhanced profile generation (new)
NTSTATUS GenerateSystemProfileEnhanced(_In_ PENHANCED_SEED_DATA enhancedSeed, _Out_ PSYSTEM_PROFILE profile);
