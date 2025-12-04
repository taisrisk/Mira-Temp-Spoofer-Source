#include "EntropyCollector.h"
#include "CryptoProvider.h"
#include <ntstrsafe.h>

// Global entropy accumulator for continuous mixing
static UINT64 g_EntropyAccumulator = 0x9E3779B97F4A7C15ULL;

//
// CollectHardwareEntropy - Gather entropy from multiple hardware sources
//
NTSTATUS CollectHardwareEntropy(_Out_ PHARDWARE_ENTROPY EntropyData)
{
    LARGE_INTEGER perfCounter, systemTime, tickCount;
    PVOID stackPtr = &EntropyData;
    PVOID poolPtr = NULL;

    if (!EntropyData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(EntropyData, sizeof(HARDWARE_ENTROPY));

    // Collect high-resolution performance counter
    KeQueryPerformanceCounter(&perfCounter);
    EntropyData->performanceCounter = perfCounter.QuadPart;

    // Collect system time
    KeQuerySystemTime(&systemTime);
    EntropyData->systemTime = systemTime.QuadPart;

    // Collect tick count
    KeQueryTickCount(&tickCount);
    EntropyData->tickCount = tickCount.QuadPart;

    // Use system time with different mixing for interrupt time simulation
    LARGE_INTEGER altTime;
    KeQuerySystemTime(&altTime);
    EntropyData->interruptTime = altTime.QuadPart ^ 0x123456789ABCDEFULL;

    // Use stack address as entropy source
    EntropyData->stackAddress = (UINT64)(ULONG_PTR)stackPtr;

    // Allocate and free pool to get memory address entropy
    poolPtr = ExAllocatePool2(POOL_FLAG_NON_PAGED, 16, 'ENTR');
    if (poolPtr) {
        EntropyData->poolAddress = (UINT64)(ULONG_PTR)poolPtr;
        ExFreePoolWithTag(poolPtr, 'ENTR');
    } else {
        EntropyData->poolAddress = (UINT64)(ULONG_PTR)&poolPtr;
    }

    // CPU cycle counter simulation
    EntropyData->cpuCycles = (UINT32)(EntropyData->performanceCounter & 0xFFFFFFFF);

    return STATUS_SUCCESS;
}

//
// MixEntropySourcesAdvanced - Advanced mixing of multiple entropy sources
//
UINT64 MixEntropySourcesAdvanced(_In_ PHARDWARE_ENTROPY EntropyData)
{
    UINT64 mixed = 0;
    UINT64 temp;

    if (!EntropyData) {
        return AdvancedHash(g_EntropyAccumulator, 3);
    }

    // Initial mixing with performance counter
    mixed = EntropyData->performanceCounter;

    // Mix in system time with rotation
    temp = EntropyData->systemTime;
    mixed ^= (temp << 23) | (temp >> 41);

    // Mix in tick count with different rotation
    temp = EntropyData->tickCount;
    mixed ^= (temp << 37) | (temp >> 27);

    // Mix in interrupt time
    mixed ^= EntropyData->interruptTime >> 13;

    // Mix in memory addresses
    mixed ^= EntropyData->stackAddress * 0x517CC1B727220A95ULL;
    mixed ^= EntropyData->poolAddress * 0x85EBCA6B3B4F4A5FULL;

    // Mix in CPU cycles
    mixed ^= (UINT64)EntropyData->cpuCycles * 0xC2B2AE3D27D4EB4FULL;

    // Apply cryptographic mixing with multiple rounds
    mixed = AdvancedHash(mixed, 7);

    // Update global accumulator for future entropy
    g_EntropyAccumulator ^= mixed;
    g_EntropyAccumulator = AdvancedHash(g_EntropyAccumulator, 3);

    return mixed;
}

//
// GenerateSecureEntropy - Generate entropy from BCrypt or hardware sources
//
NTSTATUS GenerateSecureEntropy(_Out_ PUINT64 EntropyValue)
{
    NTSTATUS status;

    if (!EntropyValue) {
        return STATUS_INVALID_PARAMETER;
    }

    // Try to use cryptographic provider first
    if (IsCryptographicProviderAvailable()) {
        status = GenerateCryptographicSeed64(EntropyValue);
        if (NT_SUCCESS(status)) {
            return status;
        }
    }

    // Fallback to hardware entropy
    LARGE_INTEGER perfCounter, systemTime;
    KeQueryPerformanceCounter(&perfCounter);
    KeQuerySystemTime(&systemTime);

    *EntropyValue = perfCounter.QuadPart ^ systemTime.QuadPart ^ (UINT64)(ULONG_PTR)EntropyValue;
    *EntropyValue = AdvancedHash(*EntropyValue, 3);

    return STATUS_SUCCESS;
}

//
// AdvancedHash - Enhanced hash function with multiple rounds
//
UINT64 AdvancedHash(_In_ UINT64 Value, _In_ UINT32 Rounds)
{
    for (UINT32 round = 0; round < Rounds; round++) {
        Value ^= (Value >> 33);
        Value *= 0xff51afd7ed558ccdULL;
        Value ^= (Value >> 33);
        Value *= 0xc4ceb9fe1a85ec53ULL;
        Value ^= (Value >> 33);

        Value ^= ((UINT64)round * 0x9E3779B97F4A7C15ULL);
    }

    return Value;
}

//
// HashValue - Simple hash function
//
UINT64 HashValue(_In_ UINT64 Value)
{
    Value ^= (Value >> 33);
    Value *= 0xff51afd7ed558ccdULL;
    Value ^= (Value >> 33);
    Value *= 0xc4ceb9fe1a85ec53ULL;
    Value ^= (Value >> 33);
    return Value;
}