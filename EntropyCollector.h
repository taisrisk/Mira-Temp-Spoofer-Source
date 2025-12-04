#pragma once

#include <ntddk.h>

// Hardware entropy sources structure
typedef struct _HARDWARE_ENTROPY {
    UINT64 performanceCounter;
    UINT64 systemTime;
    UINT64 tickCount;
    UINT64 interruptTime;
    UINT64 stackAddress;
    UINT64 poolAddress;
    UINT32 cpuCycles;
    UINT32 reserved;
} HARDWARE_ENTROPY, *PHARDWARE_ENTROPY;

// Cryptographic provider initialization
NTSTATUS InitializeCryptographicProvider(VOID);
VOID CleanupCryptographicProvider(VOID);

// Hardware entropy collection functions
NTSTATUS CollectHardwareEntropy(_Out_ PHARDWARE_ENTROPY EntropyData);
UINT64 MixEntropySourcesAdvanced(_In_ PHARDWARE_ENTROPY EntropyData);
NTSTATUS GenerateSecureEntropy(_Out_ PUINT64 EntropyValue);

// Cryptographically secure random generation
NTSTATUS GenerateCryptographicBytes(_Out_writes_bytes_(Length) PUCHAR Buffer, _In_ ULONG Length);

// Hash functions
UINT64 AdvancedHash(_In_ UINT64 Value, _In_ UINT32 Rounds);
UINT64 HashValue(_In_ UINT64 Value);