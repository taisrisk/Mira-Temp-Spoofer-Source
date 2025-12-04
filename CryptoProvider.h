#pragma once

#include <ntddk.h>

// Initialize kernel-native random number generator
NTSTATUS InitializeCryptographicProvider(VOID);

// Cleanup RNG resources
VOID CleanupCryptographicProvider(VOID);

// Generate cryptographically secure random bytes
NTSTATUS GenerateCryptographicRandomBytes(_Out_writes_bytes_(BufferSize) PUCHAR Buffer, _In_ ULONG BufferSize);

// Generate a 64-bit cryptographically secure seed
NTSTATUS GenerateCryptographicSeed64(_Out_ PUINT64 SeedValue);

// Check if provider is available
BOOLEAN IsCryptographicProviderAvailable(VOID);

// Helper functions for common use cases
UINT64 GenerateRandomUINT64(VOID);
UINT32 GenerateRandomUINT32(VOID);
UINT64 GenerateRandomRange(UINT64 min, UINT64 max);
