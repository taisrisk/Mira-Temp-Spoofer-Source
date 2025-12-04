#pragma once

#include <ntddk.h>
#include "SerialGenerator.h"

// Seed generation modes
typedef enum _SEED_GENERATION_MODE {
    SEED_MODE_TRUE_RANDOM = 0,
    SEED_MODE_USER_PROVIDED = 1,
    SEED_MODE_PLATFORM_AWARE = 2,
    SEED_MODE_TIME_BASED = 3
} SEED_GENERATION_MODE;

// Core seed generation functions
NTSTATUS GenerateTrueRandomSeed(_Out_ PENHANCED_SEED_DATA SeedData);
NTSTATUS GeneratePlatformAwareSeed(_Out_ PENHANCED_SEED_DATA SeedData, _In_opt_ PPLATFORM_INFO PlatformInfo);
NTSTATUS GenerateUserControlledSeed(_In_ UINT64 UserSeed, _Out_ PENHANCED_SEED_DATA SeedData);
NTSTATUS CreateSeedFromUserInput(_In_ PUCHAR UserData, _In_ ULONG UserDataSize, _In_opt_ PPLATFORM_INFO PlatformHint, _In_ USHORT ManufacturerHint, _Out_ PENHANCED_SEED_DATA SeedData);
NTSTATUS GenerateSeedVariant(_In_ PENHANCED_SEED_DATA OriginalSeed, _In_ UINT32 VariantIndex, _Out_ PENHANCED_SEED_DATA VariantSeed);
VOID GenerateSeedId(_In_ PENHANCED_SEED_DATA SeedData);
NTSTATUS GenerateRecommendedSeed(_In_ SEED_GENERATION_MODE Mode, _In_opt_ PUCHAR UserData, _In_ ULONG UserDataSize, _In_ USHORT ManufacturerPreference, _Out_ PENHANCED_SEED_DATA SeedData);