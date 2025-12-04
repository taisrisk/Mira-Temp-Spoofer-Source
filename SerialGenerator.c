/*
===============================================================================
HARDWARE SERIAL NUMBER GENERATION - USAGE GUIDE
===============================================================================

This module provides functions to generate and patch hardware serial numbers
such as motherboard, disk, BIOS, and chassis serial numbers. The serial numbers
are generated based on system entropy and can be patched into the SMBIOS table
for consistency across reboots.

Key Functions:

1. GenerateMotherboardSerial:
   Creates a motherboard serial number based on vendor and system entropy.
   - Vendors: ASUS, MSI, Gigabyte, ASRock, Intel, and Generic.

2. GenerateDiskSerial:
   Generates a realistic disk serial number, rotating formats between major
   manufacturers like Samsung, Western Digital, and Seagate.

3. GenerateBIOSSerial:
   Generates a BIOS serial number, with options to default to a string, be blank,
   or match the motherboard serial.

4. GenerateChassisSerial:
   Generates a chassis serial number with various default strings or a Dell-style
   format based on system entropy.

5. FindSMBIOSTable:
   Locates the SMBIOS table in physical memory to allow patching of serial numbers.

6. PatchSMBIOSString:
   Patches a string in the SMBIOS table, modifying existing serial numbers or other
   strings as needed.

7. BackupSMBIOSTables / RestoreSMBIOSTables:
   Backs up the original SMBIOS table data before patching, and restores it if needed.

8. ApplySMBIOSChanges:
   Applies all SMBIOS patches for serial numbers in one call, updating the BIOS, System,
   Baseboard, and Chassis information.

Integration with existing code requires calling ApplySMBIOSChanges() with a valid
SYSTEM_PROFILE structure populated with the desired serial numbers.

===============================================================================
*/

#include "SerialGenerator.h"
#include "EntropyCollector.h"
#include "PlatformDetector.h"
#include <ntstrsafe.h>

// Global SMBIOS backup
static PVOID g_SMBIOSBackup = NULL;
static ULONG g_SMBIOSBackupSize = 0;
static PVOID g_SMBIOSTableAddress = NULL;
static ULONG g_SMBIOSTableSize = 0;

//
// ReadSMBIOSString - Read a string from SMBIOS table
//
NTSTATUS ReadSMBIOSString(
    _In_ PVOID TableBase,
    _In_ ULONG TableSize,
    _In_ UCHAR Type,
    _In_ UCHAR StringIndex,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    PUCHAR current = (PUCHAR)TableBase;
    PUCHAR end = current + TableSize;

    if (!Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    Buffer[0] = '\0';

    while (current + sizeof(SMBIOS_STRUCTURE_HEADER) < end) {
        PSMBIOS_STRUCTURE_HEADER header = (PSMBIOS_STRUCTURE_HEADER)current;

        if (header->Type == 127) {
            break;
        }

        if (header->Type == Type) {
            PUCHAR stringSection = current + header->Length;
            UCHAR currentStringIndex = 1;

            while (stringSection < end && *stringSection != 0) {
                if (currentStringIndex == StringIndex) {
                    SIZE_T length = strlen((PCHAR)stringSection);
                    if (length > 0 && length < BufferSize) {
                        RtlStringCbCopyA(Buffer, BufferSize, (PCHAR)stringSection);
                        return STATUS_SUCCESS;
                    }
                    return STATUS_BUFFER_TOO_SMALL;
                }

                while (*stringSection != 0 && stringSection < end) {
                    stringSection++;
                }
                stringSection++;
                currentStringIndex++;
            }
            return STATUS_NOT_FOUND;
        }

        current += header->Length;
        while (current < end && !(*current == 0 && *(current + 1) == 0)) {
            current++;
        }
        current += 2;
    }

    return STATUS_NOT_FOUND;
}

//
// GenerateChassisSerialByVendor - Generate chassis serial based on vendor
//
NTSTATUS GenerateChassisSerialByVendor(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _In_opt_ PCHAR OriginalSerial,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 5);

    // If original is "Not Specified", keep it
    if (OriginalSerial && 
        (strcmp(OriginalSerial, "Not Specified") == 0 ||
         strcmp(OriginalSerial, "Default string") == 0 ||
         strcmp(OriginalSerial, "To Be Filled By O.E.M.") == 0)) {
        return RtlStringCbCopyA(Buffer, BufferSize, OriginalSerial);
    }

    switch (Vendor) {
    case VENDOR_ASUS:
        return RtlStringCbPrintfA(Buffer, BufferSize, "CSN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_MSI:
        return RtlStringCbPrintfA(Buffer, BufferSize, "CSN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_GIGABYTE:
        return RtlStringCbPrintfA(Buffer, BufferSize, "CSN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_ASROCK:
        return RtlStringCbCopyA(Buffer, BufferSize, "Not Specified");

    case VENDOR_INTEL:
        return RtlStringCbPrintfA(Buffer, BufferSize, "CSN-%010llX", hash % 0xFFFFFFFFFFULL);

    default:
        return RtlStringCbCopyA(Buffer, BufferSize, "Not Specified");
    }
}

//
// GenerateBaseboardSerialByVendor - Generate baseboard serial based on vendor
//
NTSTATUS GenerateBaseboardSerialByVendor(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 6);

    switch (Vendor) {
    case VENDOR_ASUS:
        return RtlStringCbPrintfA(Buffer, BufferSize, "MB-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_MSI:
        return RtlStringCbPrintfA(Buffer, BufferSize, "MS-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_GIGABYTE:
        return RtlStringCbPrintfA(Buffer, BufferSize, "GB-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_ASROCK:
        return RtlStringCbPrintfA(Buffer, BufferSize, "AS-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_INTEL:
        return RtlStringCbPrintfA(Buffer, BufferSize, "JKF%010llX", hash % 0xFFFFFFFFFFULL);

    default:
        return RtlStringCbPrintfA(Buffer, BufferSize, "MB-%010llX", hash % 0xFFFFFFFFFFULL);
    }
}

//
// GenerateSystemSerialByVendor - Generate system serial based on vendor
//
NTSTATUS GenerateSystemSerialByVendor(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _In_opt_ PCHAR OriginalSerial,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 7);

    // If original is "Not Specified", keep it
    if (OriginalSerial && strcmp(OriginalSerial, "Not Specified") == 0) {
        return RtlStringCbCopyA(Buffer, BufferSize, "Not Specified");
    }

    switch (Vendor) {
    case VENDOR_ASUS:
        return RtlStringCbPrintfA(Buffer, BufferSize, "SN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_MSI:
        return RtlStringCbPrintfA(Buffer, BufferSize, "MS-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_GIGABYTE:
        return RtlStringCbPrintfA(Buffer, BufferSize, "SN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_ASROCK:
        return RtlStringCbPrintfA(Buffer, BufferSize, "SN-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_INTEL:
        return RtlStringCbPrintfA(Buffer, BufferSize, "JKF%010llX", hash % 0xFFFFFFFFFFULL);

    default:
        return RtlStringCbCopyA(Buffer, BufferSize, "Not Specified");
    }
}

//
// GenerateProductSerialByVendor - Generate product serial based on vendor
//
NTSTATUS GenerateProductSerialByVendor(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _In_opt_ PCHAR OriginalSerial,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 8);

    // If original is "To Be Filled By O.E.M.", keep it
    if (OriginalSerial && strcmp(OriginalSerial, "To Be Filled By O.E.M.") == 0) {
        return RtlStringCbCopyA(Buffer, BufferSize, "To Be Filled By O.E.M.");
    }

    switch (Vendor) {
    case VENDOR_ASUS:
        return RtlStringCbPrintfA(Buffer, BufferSize, "PRD-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_MSI:
        return RtlStringCbPrintfA(Buffer, BufferSize, "MS-BW%X", (UINT32)(hash & 0xFFFF));

    case VENDOR_GIGABYTE:
        return RtlStringCbPrintfA(Buffer, BufferSize, "PRD%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_ASROCK:
        return RtlStringCbPrintfA(Buffer, BufferSize, "PRD-%010llX", hash % 0xFFFFFFFFFFULL);

    case VENDOR_INTEL:
        return RtlStringCbPrintfA(Buffer, BufferSize, "JKF%010llX", hash % 0xFFFFFFFFFFULL);

    default:
        return RtlStringCbCopyA(Buffer, BufferSize, "To Be Filled By O.E.M.");
    }
}

//
// GenerateMotherboardSerial - Legacy function for compatibility
//
NTSTATUS GenerateMotherboardSerial(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    return GenerateBaseboardSerialByVendor(Seed, Vendor, Buffer, BufferSize);
}

//
// GenerateDiskSerial - Create realistic disk serial number
//
NTSTATUS GenerateDiskSerial(
    _In_ UINT64 Seed,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 4);
    UINT32 brandSelect = (UINT32)(hash % 4);

    switch (brandSelect) {
    case 0:
        // Samsung SSD format
        return RtlStringCbPrintfA(Buffer, BufferSize, "S4N%010llX", hash % 0xFFFFFFFFFFULL);

    case 1:
        // Western Digital format
        return RtlStringCbPrintfA(Buffer, BufferSize, "WD-%012llX", hash % 0xFFFFFFFFFFFFULL);

    case 2:
        // NVMe format
        return RtlStringCbPrintfA(Buffer, BufferSize, "NVME-%010llX", hash % 0xFFFFFFFFFFULL);

    case 3:
    default:
        // SATA format
        return RtlStringCbPrintfA(Buffer, BufferSize, "SATA-%010llX", hash % 0xFFFFFFFFFFULL);
    }
}

//
// GenerateBIOSSerial - Create BIOS serial
//
NTSTATUS GenerateBIOSSerial(
    _In_ UINT64 Seed,
    _In_ MOTHERBOARD_VENDOR Vendor,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    UINT64 hash = AdvancedHash(Seed, 3);
    UINT32 choice = (UINT32)(hash % 10);

    if (choice < 5) {
        return RtlStringCbCopyA(Buffer, BufferSize, "Default string");
    }
    else if (choice < 8) {
        Buffer[0] = '\0';
        return STATUS_SUCCESS;
    }
    else {
        return GenerateBaseboardSerialByVendor(Seed, Vendor, Buffer, BufferSize);
    }
}

//
// GenerateChassisSerial - Legacy function for compatibility
//
NTSTATUS GenerateChassisSerial(
    _In_ UINT64 Seed,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize)
{
    return GenerateChassisSerialByVendor(Seed, VENDOR_GENERIC, NULL, Buffer, BufferSize);
}

//
// FindSMBIOSTable - Locate SMBIOS table in physical memory
//
NTSTATUS FindSMBIOSTable(_Out_ PVOID* TableAddress, _Out_ PULONG TableSize)
{
    NTSTATUS status = STATUS_SUCCESS;
    PHYSICAL_ADDRESS physAddr;
    PVOID mappedAddress = NULL;
    PSMBIOS_ENTRY_POINT entryPoint = NULL;
    ULONG scanSize = 0x10000;

    *TableAddress = NULL;
    *TableSize = 0;

    physAddr.QuadPart = 0xF0000;

    mappedAddress = MmMapIoSpace(physAddr, scanSize, MmNonCached);
    if (mappedAddress == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (ULONG offset = 0; offset < scanSize - sizeof(SMBIOS_ENTRY_POINT); offset += 16) {
        PUCHAR current = (PUCHAR)mappedAddress + offset;

        if (current[0] == '_' && current[1] == 'S' && current[2] == 'M' && current[3] == '_') {
            entryPoint = (PSMBIOS_ENTRY_POINT)current;

            if (entryPoint->TableAddress != 0 && entryPoint->TableLength > 0) {
                PHYSICAL_ADDRESS tablePhysAddr;
                tablePhysAddr.QuadPart = entryPoint->TableAddress;

                *TableAddress = MmMapIoSpace(tablePhysAddr, entryPoint->TableLength, MmNonCached);
                if (*TableAddress != NULL) {
                    *TableSize = entryPoint->TableLength;
                    g_SMBIOSTableAddress = *TableAddress;
                    g_SMBIOSTableSize = *TableSize;
                    status = STATUS_SUCCESS;
                }
                else {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                }
                break;
            }
        }
    }

    MmUnmapIoSpace(mappedAddress, scanSize);

    if (*TableAddress == NULL) {
        status = STATUS_NOT_FOUND;
    }

    return status;
}

//
// PatchSMBIOSString - Modify a string in SMBIOS table
//
NTSTATUS PatchSMBIOSString(
    _In_ PVOID TableBase,
    _In_ ULONG TableSize,
    _In_ UCHAR Type,
    _In_ UCHAR StringIndex,
    _In_ PCHAR NewValue)
{
    PUCHAR current = (PUCHAR)TableBase;
    PUCHAR end = current + TableSize;

    while (current + sizeof(SMBIOS_STRUCTURE_HEADER) < end) {
        PSMBIOS_STRUCTURE_HEADER header = (PSMBIOS_STRUCTURE_HEADER)current;

        if (header->Type == 127) {
            break;
        }

        if (header->Type == Type) {
            PUCHAR stringSection = current + header->Length;
            UCHAR currentStringIndex = 1;

            while (stringSection < end && *stringSection != 0) {
                if (currentStringIndex == StringIndex) {
                    SIZE_T oldLength = strlen((PCHAR)stringSection);
                    SIZE_T newLength = strlen(NewValue);

                    if (newLength <= oldLength) {
                        RtlCopyMemory(stringSection, NewValue, newLength);
                        for (SIZE_T i = newLength; i < oldLength; i++) {
                            stringSection[i] = ' ';
                        }
                        stringSection[oldLength] = '\0';
                        return STATUS_SUCCESS;
                    }
                    else {
                        RtlCopyMemory(stringSection, NewValue, oldLength);
                        stringSection[oldLength] = '\0';
                        return STATUS_SUCCESS;
                    }
                }

                while (*stringSection != 0 && stringSection < end) {
                    stringSection++;
                }
                stringSection++;
                currentStringIndex++;
            }
            return STATUS_SUCCESS;
        }

        current += header->Length;
        while (current < end && !(*current == 0 && *(current + 1) == 0)) {
            current++;
        }
        current += 2;
    }

    return STATUS_SUCCESS;
}

//
// BackupSMBIOSTables - Save original SMBIOS data
//
NTSTATUS BackupSMBIOSTables(VOID)
{
    NTSTATUS status;

    if (g_SMBIOSBackup != NULL) {
        return STATUS_SUCCESS;
    }

    status = FindSMBIOSTable(&g_SMBIOSTableAddress, &g_SMBIOSTableSize);
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: Failed to find SMBIOS table: 0x%X\n", status));
        return status;
    }

    g_SMBIOSBackup = ExAllocatePool2(POOL_FLAG_NON_PAGED, g_SMBIOSTableSize, 'SMBS');
    if (g_SMBIOSBackup == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(g_SMBIOSBackup, g_SMBIOSTableAddress, g_SMBIOSTableSize);
    g_SMBIOSBackupSize = g_SMBIOSTableSize;

    KdPrint(("PCCleanup: SMBIOS backed up (%d bytes)\n", g_SMBIOSBackupSize));
    return STATUS_SUCCESS;
}

//
// RestoreSMBIOSTables - Restore original SMBIOS data
//
NTSTATUS RestoreSMBIOSTables(VOID)
{
    if (g_SMBIOSBackup == NULL || g_SMBIOSBackupSize == 0) {
        return STATUS_SUCCESS;
    }

    RtlCopyMemory(g_SMBIOSTableAddress, g_SMBIOSBackup, g_SMBIOSBackupSize);

    ExFreePoolWithTag(g_SMBIOSBackup, 'SMBS');
    g_SMBIOSBackup = NULL;
    g_SMBIOSBackupSize = 0;

    KdPrint(("PCCleanup: SMBIOS restored\n"));
    return STATUS_SUCCESS;
}

//
// ApplySMBIOSChanges - Apply all SMBIOS patches with vendor-specific formats
//
NTSTATUS ApplySMBIOSChanges(_In_ PSYSTEM_PROFILE Profile)
{
    NTSTATUS status;
    PVOID tableAddress = NULL;
    ULONG tableSize = 0;
    CHAR originalChassisSerial[64] = { 0 };
    CHAR originalSystemSerial[64] = { 0 };
    CHAR originalProductSerial[64] = { 0 };
    PLATFORM_INFO platform;

    status = BackupSMBIOSTables();
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: SMBIOS backup failed, skipping\n"));
        return STATUS_SUCCESS;
    }

    tableAddress = g_SMBIOSTableAddress;
    tableSize = g_SMBIOSTableSize;

    if (tableAddress == NULL) {
        return STATUS_SUCCESS;
    }

    // Detect platform vendor
    status = DetectHardwarePlatform(&platform);
    if (!NT_SUCCESS(status)) {
        platform.Vendor = VENDOR_GENERIC;
    }

    // Read original values to preserve format
    ReadSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_CHASSIS, 4, originalChassisSerial, sizeof(originalChassisSerial));
    ReadSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 4, originalSystemSerial, sizeof(originalSystemSerial));
    ReadSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 6, originalProductSerial, sizeof(originalProductSerial));

    KdPrint(("PCCleanup: Original Chassis: %s\n", originalChassisSerial[0] ? originalChassisSerial : "(empty)"));
    KdPrint(("PCCleanup: Original System: %s\n", originalSystemSerial[0] ? originalSystemSerial : "(empty)"));
    KdPrint(("PCCleanup: Original Product: %s\n", originalProductSerial[0] ? originalProductSerial : "(empty)"));
    KdPrint(("PCCleanup: Applying Profile Chassis: %s\n", Profile->chassisSerial));
    KdPrint(("PCCleanup: Applying Profile Product: %s\n", Profile->productId));

    // Patch BIOS Information (Type 0)
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BIOS, 1, Profile->biosSerial);
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BIOS, 2, Profile->biosSerial);

    // Patch System Information (Type 1)
    CHAR manufacturer[64];
    switch (platform.Vendor) {
    case VENDOR_ASUS: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "ASUSTeK COMPUTER INC."); break;
    case VENDOR_MSI: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "Micro-Star International Co., Ltd."); break;
    case VENDOR_GIGABYTE: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "Gigabyte Technology Co., Ltd."); break;
    case VENDOR_ASROCK: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "ASRock"); break;
    case VENDOR_INTEL: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "Intel Corporation"); break;
    default: RtlStringCbCopyA(manufacturer, sizeof(manufacturer), "System manufacturer"); break;
    }

    // Type 1 (System Information) String Fields:
    // 1 = Manufacturer
    // 2 = Product Name  
    // 3 = Version
    // 4 = Serial Number (System Serial)
    // 5 = UUID
    // 6 = SKU Number / Product Serial
    // 7 = Family
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 1, manufacturer);
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 2, "System Product Name");
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 3, "1.0"); // Version
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 4, Profile->chassisSerial); // System Serial
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 5, Profile->systemUUID); // UUID  
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_SYSTEM, 6, Profile->productId); // SKU/Product Serial

    // Patch Baseboard Information (Type 2)
    // 1 = Manufacturer
    // 2 = Product
    // 3 = Version
    // 4 = Serial Number (Baseboard Serial)
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BASEBOARD, 1, manufacturer);
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BASEBOARD, 2, "Motherboard");
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BASEBOARD, 3, "1.0");
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_BASEBOARD, 4, Profile->motherboardSerial);

    // Patch Chassis Information (Type 3)
    // 1 = Manufacturer
    // 2 = Type (e.g., "Desktop", "Tower")
    // 3 = Version
    // 4 = Serial Number (Chassis Serial) - THIS IS THE IMPORTANT ONE
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_CHASSIS, 1, manufacturer);
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_CHASSIS, 2, "Desktop");
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_CHASSIS, 3, originalChassisSerial[0] ? originalChassisSerial : "Not Specified"); // Version - use original or default
    PatchSMBIOSString(tableAddress, tableSize, SMBIOS_TYPE_CHASSIS, 4, Profile->chassisSerial); // Chassis Serial

    KdPrint(("PCCleanup: SMBIOS patched successfully\n"));
    KdPrint(("PCCleanup: Chassis Serial set to: %s\n", Profile->chassisSerial));
    KdPrint(("PCCleanup: Product Serial set to: %s\n", Profile->productId));
    
    return STATUS_SUCCESS;
}
