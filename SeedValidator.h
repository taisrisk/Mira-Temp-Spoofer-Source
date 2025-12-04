#pragma once

#include <ntddk.h>
#include "SerialGenerator.h"
#include "PlatformDetector.h"

// Seed validation and quality functions
NTSTATUS ValidateEnhancedSeed(_In_ PENHANCED_SEED_DATA SeedData);
NTSTATUS GetSeedQualityMetrics(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG QualityScore);
NTSTATUS ValidateSeedIntegrity(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG ValidationFlags);
ULONG CompareSeedSimilarity(_In_ PENHANCED_SEED_DATA Seed1, _In_ PENHANCED_SEED_DATA Seed2);
NTSTATUS GetSeedStatistics(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG Statistics);