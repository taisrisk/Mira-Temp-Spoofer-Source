#include "SeedManager.h"
#include "EntropyCollector.h"
#include "PlatformDetector.h"
#include <ntstrsafe.h>

//
// GenerateTrueRandomSeed - Generate cryptographically random seed
//
NTSTATUS GenerateTrueRandomSeed(_Out_ PENHANCED_SEED_DATA SeedData)
{
    NTSTATUS status;
    HARDWARE_ENTROPY entropyData;
    UINT64 primaryEntropy, secondaryEntropy, tertiaryEntropy;
    LARGE_INTEGER systemTime;

    if (!SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(SeedData, sizeof(ENHANCED_SEED_DATA));

    // Collect hardware entropy multiple times for better randomness
    status = CollectHardwareEntropy(&entropyData);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    primaryEntropy = MixEntropySourcesAdvanced(&entropyData);

    // Wait briefly and collect again to get temporal variation
    KeDelayExecutionThread(KernelMode, FALSE, &(LARGE_INTEGER){.QuadPart = -10000}); // 1ms delay
    
    status = CollectHardwareEntropy(&entropyData);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    secondaryEntropy = MixEntropySourcesAdvanced(&entropyData);

    // Third collection for additional mixing
    status = CollectHardwareEntropy(&entropyData);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    tertiaryEntropy = MixEntropySourcesAdvanced(&entropyData);

    // Combine all entropy sources with advanced mixing
    SeedData->baseSeed = primaryEntropy ^ 
                        (secondaryEntropy << 21) ^ 
                        (tertiaryEntropy >> 19);
    
    SeedData->entropyMix = AdvancedHash(
        primaryEntropy ^ secondaryEntropy ^ tertiaryEntropy, 5);

    // Apply final cryptographic hash
    SeedData->baseSeed = AdvancedHash(SeedData->baseSeed, 7);

    // Set timestamp
    KeQuerySystemTime(&systemTime);
    SeedData->timestamp = systemTime.QuadPart;

    // Generate platform flags (will be random since no platform info provided)
    SeedData->platformFlags = EncodePlatformFlags(NULL);

    // Mark as not user controlled
    SeedData->userControlled = FALSE;
    SeedData->manufacturerHint = 0;

    // Generate unique seed ID
    GenerateSeedId(SeedData);

    return STATUS_SUCCESS;
}

//
// GeneratePlatformAwareSeed - Generate seed with platform encoding
//
NTSTATUS GeneratePlatformAwareSeed(_Out_ PENHANCED_SEED_DATA SeedData, _In_opt_ PPLATFORM_INFO PlatformInfo)
{
    NTSTATUS status;
    HARDWARE_ENTROPY entropyData;
    LARGE_INTEGER systemTime;

    if (!SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(SeedData, sizeof(ENHANCED_SEED_DATA));

    // Collect hardware entropy
    status = CollectHardwareEntropy(&entropyData);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Generate base seed with platform awareness
    SeedData->baseSeed = MixEntropySourcesAdvanced(&entropyData);
    SeedData->entropyMix = AdvancedHash(SeedData->baseSeed, 3);

    // Apply cryptographic hashing
    SeedData->baseSeed = AdvancedHash(SeedData->baseSeed, 5);

    // Set timestamp
    KeQuerySystemTime(&systemTime);
    SeedData->timestamp = systemTime.QuadPart;

    // Encode platform information
    SeedData->platformFlags = EncodePlatformFlags(PlatformInfo);

    // Mark as not user controlled
    SeedData->userControlled = FALSE;
    SeedData->manufacturerHint = 0;

    // Generate seed ID
    GenerateSeedId(SeedData);

    return STATUS_SUCCESS;
}

//
// GenerateUserControlledSeed - Generate seed from user input
//
NTSTATUS GenerateUserControlledSeed(_In_ UINT64 UserSeed, _Out_ PENHANCED_SEED_DATA SeedData)
{
    LARGE_INTEGER systemTime;
    HARDWARE_ENTROPY entropyData;

    if (!SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(SeedData, sizeof(ENHANCED_SEED_DATA));

    // Use user-provided seed as base
    SeedData->baseSeed = UserSeed;

    // Mix with some hardware entropy to prevent completely static values
    if (NT_SUCCESS(CollectHardwareEntropy(&entropyData))) {
        SeedData->entropyMix = MixEntropySourcesAdvanced(&entropyData);
    } else {
        SeedData->entropyMix = AdvancedHash(UserSeed, 3);
    }

    // Set timestamp
    KeQuerySystemTime(&systemTime);
    SeedData->timestamp = systemTime.QuadPart;

    // Generate platform flags with some randomness
    SeedData->platformFlags = EncodePlatformFlags(NULL);

    // Mark as user controlled
    SeedData->userControlled = TRUE;
    SeedData->manufacturerHint = 0;

    // Generate seed ID
    GenerateSeedId(SeedData);

    return STATUS_SUCCESS;
}

//
// CreateSeedFromUserInput - Create seed from user-provided data with validation
//
NTSTATUS CreateSeedFromUserInput(
    _In_ PUCHAR UserData,
    _In_ ULONG UserDataSize,
    _In_opt_ PPLATFORM_INFO PlatformHint,
    _In_ USHORT ManufacturerHint,
    _Out_ PENHANCED_SEED_DATA SeedData)
{
    NTSTATUS status;
    UINT64 userSeed;
    LARGE_INTEGER systemTime;

    if (!UserData || UserDataSize == 0 || !SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(SeedData, sizeof(ENHANCED_SEED_DATA));

    // Hash the user data to create base seed
    userSeed = 0x9E3779B97F4A7C15ULL;
    
    for (ULONG i = 0; i < UserDataSize; i++) {
        userSeed ^= UserData[i];
        userSeed = HashValue(userSeed);
    }

    // Apply advanced hashing for better distribution
    SeedData->baseSeed = AdvancedHash(userSeed, 7);

    // Mix in some hardware entropy to prevent completely static values
    HARDWARE_ENTROPY hwEntropy;
    status = CollectHardwareEntropy(&hwEntropy);
    if (NT_SUCCESS(status)) {
        SeedData->entropyMix = MixEntropySourcesAdvanced(&hwEntropy);
        SeedData->entropyMix = AdvancedHash(SeedData->entropyMix ^ userSeed, 3);
    } else {
        SeedData->entropyMix = AdvancedHash(userSeed ^ 0x517CC1B727220A95ULL, 3);
    }

    // Set timestamp
    KeQuerySystemTime(&systemTime);
    SeedData->timestamp = systemTime.QuadPart;

    // Encode platform flags
    SeedData->platformFlags = EncodePlatformFlags(PlatformHint);

    // Mark as user controlled
    SeedData->userControlled = TRUE;
    SeedData->manufacturerHint = ManufacturerHint;

    // Generate seed ID
    GenerateSeedId(SeedData);

    return STATUS_SUCCESS;
}

//
// GenerateSeedVariant - Create variant of existing seed for different purposes
//
NTSTATUS GenerateSeedVariant(
    _In_ PENHANCED_SEED_DATA OriginalSeed,
    _In_ UINT32 VariantIndex,
    _Out_ PENHANCED_SEED_DATA VariantSeed)
{
    UINT64 variantHash;

    if (!OriginalSeed || !VariantSeed) {
        return STATUS_INVALID_PARAMETER;
    }

    // Copy original seed as base
    RtlCopyMemory(VariantSeed, OriginalSeed, sizeof(ENHANCED_SEED_DATA));

    // Create variant by mixing original seed with variant index
    variantHash = AdvancedHash(OriginalSeed->baseSeed ^ ((UINT64)VariantIndex * 0x9E3779B97F4A7C15ULL), 5);
    
    VariantSeed->baseSeed = variantHash;
    VariantSeed->entropyMix = AdvancedHash(OriginalSeed->entropyMix ^ variantHash, 3);

    // Update timestamp to current time
    LARGE_INTEGER systemTime;
    KeQuerySystemTime(&systemTime);
    VariantSeed->timestamp = systemTime.QuadPart;

    // Generate new seed ID for variant
    GenerateSeedId(VariantSeed);

    return STATUS_SUCCESS;
}

//
// GenerateSeedId - Generate unique identifier for seed
//
VOID GenerateSeedId(_In_ PENHANCED_SEED_DATA SeedData)
{
    UINT64 idHash;

    if (!SeedData) {
        return;
    }

    // Create ID from seed components
    idHash = SeedData->baseSeed ^ SeedData->timestamp ^ SeedData->entropyMix;
    idHash = AdvancedHash(idHash, 4);

    // Convert to readable hex string (first 12 characters)
    RtlStringCbPrintfA(SeedData->seedId, sizeof(SeedData->seedId),
        "%012llX", idHash & 0xFFFFFFFFFFFFULL);
}

//
// GenerateRecommendedSeed - Generate seed based on user preferences and system capabilities
//
NTSTATUS GenerateRecommendedSeed(
    _In_ SEED_GENERATION_MODE Mode,
    _In_opt_ PUCHAR UserData,
    _In_ ULONG UserDataSize,
    _In_ USHORT ManufacturerPreference,
    _Out_ PENHANCED_SEED_DATA SeedData)
{
    NTSTATUS status;
    PLATFORM_INFO platformInfo;

    if (!SeedData) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Mode) {
    case SEED_MODE_TRUE_RANDOM:
        status = GenerateTrueRandomSeed(SeedData);
        if (NT_SUCCESS(status) && ManufacturerPreference != 0) {
            SeedData->manufacturerHint = ManufacturerPreference;
            GenerateSeedId(SeedData);
        }
        break;

    case SEED_MODE_USER_PROVIDED:
        if (!UserData || UserDataSize == 0) {
            return STATUS_INVALID_PARAMETER;
        }
        
        status = DetectHardwarePlatform(&platformInfo);
        if (!NT_SUCCESS(status)) {
            RtlZeroMemory(&platformInfo, sizeof(platformInfo));
        }
        
        status = CreateSeedFromUserInput(UserData, UserDataSize, &platformInfo, ManufacturerPreference, SeedData);
        break;

    case SEED_MODE_PLATFORM_AWARE:
        status = DetectHardwarePlatform(&platformInfo);
        if (!NT_SUCCESS(status)) {
            status = GenerateTrueRandomSeed(SeedData);
        } else {
            status = GeneratePlatformAwareSeed(SeedData, &platformInfo);
        }
        
        if (NT_SUCCESS(status) && ManufacturerPreference != 0) {
            SeedData->manufacturerHint = ManufacturerPreference;
        }
        break;

    case SEED_MODE_TIME_BASED:
        LARGE_INTEGER systemTime;
        KeQuerySystemTime(&systemTime);
        
        UINT64 timeSeed = systemTime.QuadPart;
        if (UserData && UserDataSize > 0) {
            for (ULONG i = 0; i < UserDataSize; i++) {
                timeSeed ^= UserData[i];
                timeSeed = HashValue(timeSeed);
            }
        }
        
        status = GenerateUserControlledSeed(timeSeed, SeedData);
        if (NT_SUCCESS(status) && ManufacturerPreference != 0) {
            SeedData->manufacturerHint = ManufacturerPreference;
        }
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return status;
}