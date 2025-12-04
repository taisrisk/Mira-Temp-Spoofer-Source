#include "SeedPersistence.h"
#include <ntstrsafe.h>

//
// SaveSeedToBuffer - Save enhanced seed data to buffer for persistence
//
NTSTATUS SaveSeedToBuffer(_In_ PENHANCED_SEED_DATA SeedData, _Out_ PUCHAR Buffer, _In_ ULONG BufferSize, _Out_ PULONG RequiredSize)
{
    ULONG neededSize;
    PUCHAR currentPtr;

    if (!SeedData || !RequiredSize) {
        return STATUS_INVALID_PARAMETER;
    }

    neededSize = sizeof(ENHANCED_SEED_DATA) + sizeof(UINT32) + sizeof(UINT32);
    *RequiredSize = neededSize;

    if (!Buffer || BufferSize < neededSize) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    currentPtr = Buffer;

    // Write version marker
    *(PUINT32)currentPtr = 0x53454544; // 'SEED' marker
    currentPtr += sizeof(UINT32);

    // Write seed data
    RtlCopyMemory(currentPtr, SeedData, sizeof(ENHANCED_SEED_DATA));
    currentPtr += sizeof(ENHANCED_SEED_DATA);

    // Calculate and write checksum
    UINT32 checksum = 0;
    PUCHAR seedBytes = (PUCHAR)SeedData;
    for (ULONG i = 0; i < sizeof(ENHANCED_SEED_DATA); i++) {
        checksum ^= seedBytes[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    *(PUINT32)currentPtr = checksum;

    return STATUS_SUCCESS;
}

//
// LoadSeedFromBuffer - Load enhanced seed data from buffer
//
NTSTATUS LoadSeedFromBuffer(_In_ PUCHAR Buffer, _In_ ULONG BufferSize, _Out_ PENHANCED_SEED_DATA SeedData)
{
    PUCHAR currentPtr;
    UINT32 version, checksum, calculatedChecksum;
    ULONG expectedSize;

    if (!Buffer || !SeedData || BufferSize < sizeof(UINT32) * 2 + sizeof(ENHANCED_SEED_DATA)) {
        return STATUS_INVALID_PARAMETER;
    }

    currentPtr = Buffer;
    expectedSize = sizeof(ENHANCED_SEED_DATA) + sizeof(UINT32) + sizeof(UINT32);

    if (BufferSize < expectedSize) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Check version marker
    version = *(PUINT32)currentPtr;
    currentPtr += sizeof(UINT32);

    if (version != 0x53454544) { // 'SEED' marker
        return STATUS_INVALID_PARAMETER;
    }

    // Copy seed data
    RtlCopyMemory(SeedData, currentPtr, sizeof(ENHANCED_SEED_DATA));
    currentPtr += sizeof(ENHANCED_SEED_DATA);

    // Verify checksum
    checksum = *(PUINT32)currentPtr;
    calculatedChecksum = 0;
    PUCHAR seedBytes = (PUCHAR)SeedData;
    for (ULONG i = 0; i < sizeof(ENHANCED_SEED_DATA); i++) {
        calculatedChecksum ^= seedBytes[i];
        calculatedChecksum = (calculatedChecksum << 1) | (calculatedChecksum >> 31);
    }

    if (checksum != calculatedChecksum) {
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}