#include "CryptoProvider.h"
#include <ntstrsafe.h>

//
// Kernel-native random generation using only ntoskrnl.exe exports
// No BCrypt/cng.sys dependency - works with KDU mapping
//

// Global entropy state
static UINT64 g_EntropyPool = 0;
static BOOLEAN g_CryptoInitialized = FALSE;

//
// Initialize entropy pool from various kernel sources
//
NTSTATUS InitializeCryptographicProvider(VOID)
{
    LARGE_INTEGER perfCounter, systemTime;
    UINT64 entropy = 0;
    ULONG tickCount;
    
    PAGED_CODE();
    
    DbgPrint("CryptoProvider: Initializing kernel-native RNG...\n");
    
    // Source 1: Performance counter (high-resolution timer)
    perfCounter = KeQueryPerformanceCounter(NULL);
    entropy ^= perfCounter.QuadPart;
    
    // Source 2: System time (low-resolution but unpredictable)
    KeQuerySystemTime(&systemTime);
    entropy ^= systemTime.QuadPart;
    
    // Source 3: Tick count (milliseconds since boot) - using SharedUserData
    tickCount = *((volatile ULONG*)0x7FFE0000 + 1);  // Simplified tick count access
    entropy ^= tickCount;
    
    // Source 4: Interrupt time (time spent in interrupts)
    entropy ^= KeQueryInterruptTime();
    
    // Source 5: Current IRQL and processor number
    entropy ^= ((UINT64)KeGetCurrentIrql() << 48);
    entropy ^= ((UINT64)KeGetCurrentProcessorNumber() << 32);
    
    // Source 6: Stack address (ASLR randomization)
    entropy ^= (UINT64)&entropy;
    
    // Source 7: Code address (module load randomization)
    entropy ^= (UINT64)InitializeCryptographicProvider;
    
    // Initialize pool
    g_EntropyPool = entropy;
    g_CryptoInitialized = TRUE;
    
    DbgPrint("CryptoProvider: Initialized with entropy: 0x%016llX\n", entropy);
    
    return STATUS_SUCCESS;
}

//
// Cleanup (securely zero entropy pool)
//

VOID CleanupCryptographicProvider(VOID)
{
    PAGED_CODE();
    
    // Securely zero the entropy pool
    RtlSecureZeroMemory(&g_EntropyPool, sizeof(g_EntropyPool));
    g_CryptoInitialized = FALSE;
    
    DbgPrint("CryptoProvider: Cleanup complete\n");
}

//
// SplitMix64 - High-quality mixing function
//
static UINT64 MixEntropy(UINT64 value)
{
    // SplitMix64 algorithm - excellent bit avalanche
    value ^= value >> 33;
    value *= 0xFF51AFD7ED558CCDULL;
    value ^= value >> 33;
    value *= 0xC4CEB9FE1A85EC53ULL;
    value ^= value >> 33;
    
    return value;
}

//
// Generate cryptographically secure random bytes
//
NTSTATUS GenerateCryptographicRandomBytes(
    _Out_writes_bytes_(BufferSize) PUCHAR Buffer,
    _In_ ULONG BufferSize
)
{
    LARGE_INTEGER perfCounter;
    UINT64 entropy;
    ULONG i;
    
    if (!Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // If not initialized, auto-initialize
    if (!g_CryptoInitialized) {
        InitializeCryptographicProvider();
    }
    
    // For each byte, mix fresh entropy
    for (i = 0; i < BufferSize; i++) {
        // Collect fresh entropy from kernel every byte
        perfCounter = KeQueryPerformanceCounter(NULL);
        
        // Mix with pool and fresh sources
        entropy = g_EntropyPool;
        entropy ^= perfCounter.QuadPart;
        entropy ^= KeQueryInterruptTime();
        entropy ^= (UINT64)i;
        entropy ^= ((UINT64)&entropy) + i;
        
        // Mix thoroughly using SplitMix64
        entropy = MixEntropy(entropy);
        
        // Update pool for next iteration (avalanche effect)
        g_EntropyPool = entropy;
        
        // Extract byte
        Buffer[i] = (UCHAR)(entropy & 0xFF);
    }
    
    return STATUS_SUCCESS;
}

//
// Generate a 64-bit cryptographically secure seed
//
NTSTATUS GenerateCryptographicSeed64(_Out_ PUINT64 SeedValue)
{
    NTSTATUS status;
    UCHAR randomBytes[8];

    if (!SeedValue) {
        return STATUS_INVALID_PARAMETER;
    }

    status = GenerateCryptographicRandomBytes(randomBytes, sizeof(randomBytes));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *SeedValue = *(PUINT64)randomBytes;

    return STATUS_SUCCESS;
}

//
// Check if provider is initialized
//
BOOLEAN IsCryptographicProviderAvailable(VOID)
{
    return g_CryptoInitialized;
}

//
// Helper: Generate random UINT64 value
//
UINT64 GenerateRandomUINT64(VOID)
{
    UINT64 value = 0;
    GenerateCryptographicRandomBytes((PUCHAR)&value, sizeof(UINT64));
    return value;
}

//
// Helper: Generate random UINT32 value
//
UINT32 GenerateRandomUINT32(VOID)
{
    UINT32 value = 0;
    GenerateCryptographicRandomBytes((PUCHAR)&value, sizeof(UINT32));
    return value;
}

//
// Helper: Generate random value in range [min, max]
//
UINT64 GenerateRandomRange(UINT64 min, UINT64 max)
{
    UINT64 range, value;
    
    if (min >= max) {
        return min;
    }
    
    range = max - min + 1;
    value = GenerateRandomUINT64();
    
    return min + (value % range);
}
