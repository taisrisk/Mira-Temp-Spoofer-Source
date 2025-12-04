#pragma once

#include <ntddk.h>

// Forward declarations - we'll include the actual definitions through SerialGenerator.h
struct _ENHANCED_SEED_DATA;
struct _HARDWARE_ENTROPY;

// GUID/UUID generation functions
VOID GenerateGUID(_In_ UINT64 Seed, _Out_ PCHAR GuidString, _In_ SIZE_T BufferSize);
VOID GenerateUUID(_In_ UINT64 Seed, _Out_ PCHAR UuidString, _In_ SIZE_T BufferSize);
NTSTATUS GenerateProductId(_In_ UINT64 Seed, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateMACAddress(_In_ UINT64 Seed, _Out_writes_bytes_(6) PUCHAR MacAddress);

// Enhanced entropy and randomness functions
NTSTATUS GenerateSecureRandom(_Out_ PVOID Buffer, _In_ ULONG BufferSize);
NTSTATUS GenerateSecureRandomEnhanced(_Out_ PVOID Buffer, _In_ ULONG BufferSize, _In_opt_ struct _ENHANCED_SEED_DATA* SeedSource);
NTSTATUS CollectSystemEntropy(_Out_ struct _HARDWARE_ENTROPY* EntropyData);
UINT64 MixMultipleEntropySources(_In_ PUINT64 EntropySources, _In_ ULONG SourceCount);

// Cryptographic quality random generation
NTSTATUS GenerateCryptographicSeed(_Out_ PUINT64 SeedValue);
NTSTATUS GenerateHighQualityBytes(_Out_ PUCHAR Buffer, _In_ ULONG BufferSize);

// Seed conversion utilities
NTSTATUS ConvertEnhancedSeedToLegacy(_In_ struct _ENHANCED_SEED_DATA* EnhancedSeed, _Out_ PUINT64 LegacySeed);
