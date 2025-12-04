#include "GuidGenerator.h"
#include "EntropyCollector.h"
#include "SerialGenerator.h"
#include "CryptoProvider.h"
#include <ntstrsafe.h>

//
// EnhancedHash - Simple wrapper for AdvancedHash with default rounds
//
UINT64 EnhancedHash(_In_ UINT64 Value)
{
    return AdvancedHash(Value, 5);
}

//
// CollectSystemEntropy - Wrapper for hardware entropy collection
//
NTSTATUS CollectSystemEntropy(_Out_ PHARDWARE_ENTROPY EntropyData)
{
    return CollectHardwareEntropy(EntropyData);
}

//
// MixMultipleEntropySources - Mix array of entropy sources
//
UINT64 MixMultipleEntropySources(_In_ PUINT64 EntropySources, _In_ ULONG SourceCount)
{
    UINT64 mixed = 0x9E3779B97F4A7C15ULL;
    UINT32 i;

    if (!EntropySources || SourceCount == 0) {
        return HashValue(mixed);
    }

    for (i = 0; i < SourceCount; i++) {
        UINT64 source = EntropySources[i];
        UINT32 rotation = (i * 7 + 13) % 64;
        
        UINT64 rotated = (source << rotation) | (source >> (64 - rotation));
        
        mixed ^= rotated * (0xff51afd7ed558ccdULL + i * 0x517CC1B727220A95ULL);
        mixed = HashValue(mixed);
    }

    mixed = EnhancedHash(mixed);
    return mixed;
}

//
// GenerateCryptographicSeed - Generate high-quality cryptographic seed using BCrypt
//
NTSTATUS GenerateCryptographicSeed(_Out_ PUINT64 SeedValue)
{
    NTSTATUS status;

    if (!SeedValue) {
        return STATUS_INVALID_PARAMETER;
    }

    // Use cryptographic provider if available
    if (IsCryptographicProviderAvailable()) {
        status = GenerateCryptographicSeed64(SeedValue);
        if (NT_SUCCESS(status)) {
            return status;
        }
    }

    // Fallback to hardware entropy collection
    HARDWARE_ENTROPY entropy1, entropy2, entropy3;
    UINT64 entropySources[9];
    LARGE_INTEGER delay;

    status = CollectSystemEntropy(&entropy1);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    delay.QuadPart = -5000;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    status = CollectSystemEntropy(&entropy2);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    delay.QuadPart = -3000;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    status = CollectSystemEntropy(&entropy3);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    entropySources[0] = entropy1.performanceCounter;
    entropySources[1] = entropy1.systemTime;
    entropySources[2] = entropy1.stackAddress;
    entropySources[3] = entropy2.performanceCounter;
    entropySources[4] = entropy2.interruptTime;
    entropySources[5] = entropy2.poolAddress;
    entropySources[6] = entropy3.performanceCounter;
    entropySources[7] = entropy3.tickCount;
    entropySources[8] = entropy3.stackAddress;

    *SeedValue = MixMultipleEntropySources(entropySources, 9);
    *SeedValue = AdvancedHash(*SeedValue, 8);

    return STATUS_SUCCESS;
}

//
// GenerateHighQualityBytes - Generate cryptographically strong random bytes using BCrypt
//
NTSTATUS GenerateHighQualityBytes(_Out_ PUCHAR Buffer, _In_ ULONG BufferSize)
{
    if (!Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Use BCrypt directly for high-quality random bytes
    return GenerateCryptographicRandomBytes(Buffer, BufferSize);
}

//
// GenerateSecureRandomEnhanced - Enhanced version using BCrypt
//
NTSTATUS GenerateSecureRandomEnhanced(_Out_ PVOID Buffer, _In_ ULONG BufferSize, _In_opt_ PENHANCED_SEED_DATA SeedSource)
{
    NTSTATUS status;
    PUCHAR byteBuffer = (PUCHAR)Buffer;
    UINT64 seed;
    
    if (!Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // If we have BCrypt available, use it directly
    if (IsCryptographicProviderAvailable() && SeedSource == NULL) {
        return GenerateCryptographicRandomBytes(byteBuffer, BufferSize);
    }

    // Otherwise use seed-based generation
    if (SeedSource && SeedSource->baseSeed != 0) {
        seed = SeedSource->baseSeed ^ SeedSource->entropyMix;
    } else {
        status = GenerateCryptographicSeed(&seed);
        if (!NT_SUCCESS(status)) {
            HARDWARE_ENTROPY entropy;
            status = CollectSystemEntropy(&entropy);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            seed = MixEntropySourcesAdvanced(&entropy);
        }
    }

    for (ULONG i = 0; i < BufferSize; i++) {
        seed = AdvancedHash(seed + i, 5);
        byteBuffer[i] = (UCHAR)(seed & 0xFF);
        
        if (i > 0 && i % 64 == 0) {
            UINT64 newEntropy;
            if (NT_SUCCESS(GenerateCryptographicSeed(&newEntropy))) {
                seed ^= newEntropy;
            }
        }
    }
    
    return STATUS_SUCCESS;
}

//
// GenerateSecureRandom - Generate secure random bytes (legacy function)
//

NTSTATUS GenerateSecureRandom(_Out_ PVOID Buffer, _In_ ULONG BufferSize)
{
    return GenerateSecureRandomEnhanced(Buffer, BufferSize, NULL);
}

//
// ConvertEnhancedSeedToLegacy - Convert enhanced seed to legacy format
//
NTSTATUS ConvertEnhancedSeedToLegacy(_In_ PENHANCED_SEED_DATA EnhancedSeed, _Out_ PUINT64 LegacySeed)
{
    if (!EnhancedSeed || !LegacySeed) {
        return STATUS_INVALID_PARAMETER;
    }

    *LegacySeed = EnhancedSeed->baseSeed ^ 
                  (EnhancedSeed->entropyMix >> 16) ^ 
                  ((UINT64)EnhancedSeed->platformFlags << 32);

    return STATUS_SUCCESS;
}

//
// GenerateGUID - Create GUID string from seed
//
VOID GenerateGUID(_In_ UINT64 Seed, _Out_ PCHAR GuidString, _In_ SIZE_T BufferSize)
{
    UINT64 hash1 = HashValue(Seed);
    UINT64 hash2 = HashValue(Seed ^ 0xDEADBEEFCAFEBABEULL);

    UINT32 d1 = (UINT32)(hash1 & 0xFFFFFFFF);
    UINT16 d2 = (UINT16)((hash1 >> 32) & 0xFFFF);
    UINT16 d3 = (UINT16)((hash1 >> 48) & 0xFFFF);
    UINT16 d4 = (UINT16)(hash2 & 0xFFFF);
    UINT64 d5 = (hash2 >> 16) & 0xFFFFFFFFFFFFULL;

    RtlStringCbPrintfA(GuidString, BufferSize,
        "{%08X-%04X-%04X-%04X-%012llX}",
        d1, d2, d3, d4, d5);
}

//
// GenerateUUID - Create UUID string from seed (RFC 4122 format)
//

VOID GenerateUUID(_In_ UINT64 Seed, _Out_ PCHAR UuidString, _In_ SIZE_T BufferSize)
{
    UINT64 hash1 = HashValue(Seed);
    UINT64 hash2 = HashValue(Seed ^ 0xDEADBEEFCAFEBABEULL);

    UINT32 d1 = (UINT32)(hash1 & 0xFFFFFFFF);
    UINT16 d2 = (UINT16)((hash1 >> 32) & 0xFFFF);
    UINT16 d3 = (UINT16)((hash1 >> 48) & 0x0FFF) | 0x4000;
    UINT16 d4 = (UINT16)((hash2 & 0x3FFF) | 0x8000);
    UINT64 d5 = (hash2 >> 16) & 0xFFFFFFFFFFFFULL;

    RtlStringCbPrintfA(UuidString, BufferSize,
        "%08X-%04X-%04X-%04X-%012llX",
        d1, d2, d3, d4, d5);
}

//
// GenerateProductId - Generate Windows Product ID
//
NTSTATUS GenerateProductId(_In_ UINT64 Seed, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize)
{
    UINT64 hash = HashValue(Seed);

    return RtlStringCbPrintfA(Buffer, BufferSize,
        "%05llu-%05llu-%05llu-%05llu",
        (HashValue(hash) % 100000),
        (HashValue(hash * 2) % 100000),
        (HashValue(hash * 3) % 100000),
        (HashValue(hash * 4) % 100000));
}

//
// GenerateMACAddress - Generate locally administered MAC address
//
NTSTATUS GenerateMACAddress(_In_ UINT64 Seed, _Out_writes_bytes_(6) PUCHAR MacAddress)
{
    UINT64 hash = HashValue(Seed ^ 0xBADC0FFEE0DDF00DULL);

    if (MacAddress == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    MacAddress[0] = 0x02;
    MacAddress[1] = (UCHAR)((hash >> 40) & 0xFF);
    MacAddress[2] = (UCHAR)((hash >> 32) & 0xFF);
    MacAddress[3] = (UCHAR)((hash >> 24) & 0xFF);
    MacAddress[4] = (UCHAR)((hash >> 16) & 0xFF);
    MacAddress[5] = (UCHAR)((hash >> 8) & 0xFF);

    return STATUS_SUCCESS;
}
