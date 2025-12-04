#pragma once

#include <ntddk.h>
#include "SerialGenerator.h"

// Seed persistence functions
NTSTATUS SaveSeedToBuffer(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PUCHAR Buffer, _In_ ULONG BufferSize, _Out_ PULONG RequiredSize);
NTSTATUS LoadSeedFromBuffer(_In_ PUCHAR Buffer, _In_ ULONG BufferSize, _Out_ PENHANCED_SEED_DATA SeedData);