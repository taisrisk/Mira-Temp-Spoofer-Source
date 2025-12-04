#include "SeedValidator.h"
#include "EntropyCollector.h"
#include <ntstrsafe.h>

//
// ValidateEnhancedSeed - Validate seed data integrity
//
NTSTATUS ValidateEnhancedSeed(_In_ PENHANCED_SEED_DATA SeedData)
{
    if (!SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    if (SeedData->baseSeed == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (SeedData->timestamp == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (SeedData->seedId[0] == '\0') {
        // Generate missing seed ID
        UINT64 idHash = SeedData->baseSeed ^ SeedData->timestamp ^ SeedData->entropyMix;
        idHash = AdvancedHash(idHash, 4);
        RtlStringCbPrintfA(SeedData->seedId, sizeof(SeedData->seedId),
            "%012llX", idHash & 0xFFFFFFFFFFFFULL);
    }

    return STATUS_SUCCESS;
}

//
// GetSeedQualityMetrics - Calculate seed quality score
//
NTSTATUS GetSeedQualityMetrics(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG QualityScore)
{
    ULONG score = 0;
    UINT64 seed, entropy;
    UINT32 bits;

    if (!SeedData || !QualityScore) {
        return STATUS_INVALID_PARAMETER;
    }

    *QualityScore = 0;

    seed = SeedData->baseSeed;
    entropy = SeedData->entropyMix;

    // Score based on bit distribution in base seed
    bits = 0;
    for (int i = 0; i < 64; i++) {
        if (seed & (1ULL << i)) bits++;
    }
    
    if (bits >= 24 && bits <= 40) {
        score += 25;
    }

    // Score based on entropy mix
    bits = 0;
    for (int i = 0; i < 64; i++) {
        if (entropy & (1ULL << i)) bits++;
    }
    
    if (bits >= 24 && bits <= 40) {
        score += 25;
    }

    // Score based on timestamp freshness
    LARGE_INTEGER currentTime;
    KeQuerySystemTime(&currentTime);
    UINT64 timeDiff = currentTime.QuadPart - SeedData->timestamp;
    
    if (timeDiff < 36000000000ULL) {
        score += 20;
    }

    // Score based on user control
    if (!SeedData->userControlled) {
        score += 30;
    } else {
        score += 10;
    }

    *QualityScore = score;
    return STATUS_SUCCESS;
}

//
// ValidateSeedIntegrity - Advanced validation of seed integrity
//
NTSTATUS ValidateSeedIntegrity(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG ValidationFlags)
{
    ULONG flags = 0;
    UINT64 seed;
    UINT32 bits;

    if (!SeedData || !ValidationFlags) {
        return STATUS_INVALID_PARAMETER;
    }

    *ValidationFlags = 0;

    if (SeedData->baseSeed == 0) {
        flags |= 0x01;
    }

    if (SeedData->entropyMix == 0) {
        flags |= 0x02;
    }

    LARGE_INTEGER currentTime;
    KeQuerySystemTime(&currentTime);
    UINT64 timeDiff = currentTime.QuadPart - SeedData->timestamp;
    
    if (timeDiff > 31536000000000ULL) {
        flags |= 0x04;
    }

    seed = SeedData->baseSeed;
    bits = 0;
    for (int i = 0; i < 64; i++) {
        if (seed & (1ULL << i)) bits++;
    }
    if (bits < 16 || bits > 48) {
        flags |= 0x08;
    }

    UINT32 vendor = (SeedData->platformFlags & PLATFORM_VENDOR_MASK) >> PLATFORM_VENDOR_SHIFT;
    UINT32 month = (SeedData->platformFlags & PLATFORM_MONTH_MASK) >> PLATFORM_MONTH_SHIFT;
    
    if (vendor > 5 || month == 0 || month > 12) {
        flags |= 0x10;
    }

    BOOLEAN validId = TRUE;
    for (int i = 0; i < 12; i++) {
        CHAR c = SeedData->seedId[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
            validId = FALSE;
            break;
        }
    }
    if (!validId || SeedData->seedId[12] != '\0') {
        flags |= 0x20;
    }

    *ValidationFlags = flags;

    if (flags == 0) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_DATA_ERROR;
    }
}

//
// CompareSeedSimilarity - Compare two seeds and return similarity score
//
ULONG CompareSeedSimilarity(_In_ PENHANCED_SEED_DATA Seed1, _In_ PENHANCED_SEED_DATA Seed2)
{
    ULONG similarity = 0;
    UINT64 xorResult;
    UINT32 commonBits;

    if (!Seed1 || !Seed2) {
        return 0;
    }

    // Compare base seeds
    xorResult = Seed1->baseSeed ^ Seed2->baseSeed;
    commonBits = 0;
    for (int i = 0; i < 64; i++) {
        if ((xorResult & (1ULL << i)) == 0) {
            commonBits++;
        }
    }
    similarity += (commonBits * 30) / 64;

    // Compare entropy mix
    xorResult = Seed1->entropyMix ^ Seed2->entropyMix;
    commonBits = 0;
    for (int i = 0; i < 64; i++) {
        if ((xorResult & (1ULL << i)) == 0) {
            commonBits++;
        }
    }
    similarity += (commonBits * 20) / 64;

    // Compare platform flags
    if (Seed1->platformFlags == Seed2->platformFlags) {
        similarity += 25;
    } else {
        UINT32 flagDiff = Seed1->platformFlags ^ Seed2->platformFlags;
        UINT32 differentBits = 0;
        for (int i = 0; i < 32; i++) {
            if (flagDiff & (1UL << i)) {
                differentBits++;
            }
        }
        similarity += (25 * (32 - differentBits)) / 32;
    }

    if (Seed1->userControlled == Seed2->userControlled) {
        similarity += 10;
    }

    if (Seed1->manufacturerHint == Seed2->manufacturerHint) {
        similarity += 15;
    }

    return similarity;
}

//
// GetSeedStatistics - Get detailed statistics about a seed
//
NTSTATUS GetSeedStatistics(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PULONG Statistics)
{
    ULONG stats[8];
    UINT64 seed, entropy;
    UINT32 bits;

    if (!SeedData || !Statistics) {
        return STATUS_INVALID_PARAMETER;
    }

    seed = SeedData->baseSeed;
    entropy = SeedData->entropyMix;

    // Stat 0: Number of set bits in base seed
    bits = 0;
    for (int i = 0; i < 64; i++) {
        if (seed & (1ULL << i)) bits++;
    }
    stats[0] = bits;

    // Stat 1: Number of set bits in entropy mix
    bits = 0;
    for (int i = 0; i < 64; i++) {
        if (entropy & (1ULL << i)) bits++;
    }
    stats[1] = bits;

    stats[2] = (SeedData->platformFlags & PLATFORM_VENDOR_MASK) >> PLATFORM_VENDOR_SHIFT;
    stats[3] = (SeedData->platformFlags & PLATFORM_CPU_INTEL) ? 1 : 0;
    stats[4] = (SeedData->platformFlags & PLATFORM_YEAR_MASK) >> PLATFORM_YEAR_SHIFT;
    stats[5] = (SeedData->platformFlags & PLATFORM_MONTH_MASK) >> PLATFORM_MONTH_SHIFT;
    stats[6] = SeedData->userControlled ? 1 : 0;
    stats[7] = SeedData->manufacturerHint;

    RtlCopyMemory(Statistics, stats, sizeof(stats));

    return STATUS_SUCCESS;
}