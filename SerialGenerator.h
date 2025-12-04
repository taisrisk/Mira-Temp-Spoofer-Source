#pragma once

#include <ntddk.h>
#include "Driver.h"
#include "PlatformDetector.h"

// Enhanced seed structure with platform awareness
typedef struct _ENHANCED_SEED_DATA {
    UINT64 baseSeed;
    UINT64 timestamp;
    UINT64 entropyMix;
    UINT32 platformFlags;
    UINT16 manufacturerHint;
    UINT16 reserved;
    BOOLEAN userControlled;
    CHAR seedId[16];
} ENHANCED_SEED_DATA, *PENHANCED_SEED_DATA;

// SMBIOS structure headers
#pragma pack(push, 1)
typedef struct _SMBIOS_ENTRY_POINT {
    CHAR AnchorString[4];
    UCHAR Checksum;
    UCHAR Length;
    UCHAR MajorVersion;
    UCHAR MinorVersion;
    USHORT MaxStructureSize;
    UCHAR EntryPointRevision;
    CHAR FormattedArea[5];
    CHAR IntermediateAnchor[5];
    UCHAR IntermediateChecksum;
    USHORT TableLength;
    ULONG TableAddress;
    USHORT NumberOfStructures;
    UCHAR BCDRevision;
} SMBIOS_ENTRY_POINT, * PSMBIOS_ENTRY_POINT;

typedef struct _SMBIOS_STRUCTURE_HEADER {
    UCHAR Type;
    UCHAR Length;
    USHORT Handle;
} SMBIOS_STRUCTURE_HEADER, * PSMBIOS_STRUCTURE_HEADER;
#pragma pack(pop)

// SMBIOS Type definitions
#define SMBIOS_TYPE_BIOS 0
#define SMBIOS_TYPE_SYSTEM 1
#define SMBIOS_TYPE_BASEBOARD 2
#define SMBIOS_TYPE_CHASSIS 3

// Vendor-specific serial generation functions
NTSTATUS GenerateChassisSerialByVendor(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _In_opt_ PCHAR OriginalSerial, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateBaseboardSerialByVendor(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateSystemSerialByVendor(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _In_opt_ PCHAR OriginalSerial, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateProductSerialByVendor(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _In_opt_ PCHAR OriginalSerial, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);

// Legacy serial generation functions (for compatibility)
NTSTATUS GenerateMotherboardSerial(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateDiskSerial(_In_ UINT64 Seed, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateBIOSSerial(_In_ UINT64 Seed, _In_ MOTHERBOARD_VENDOR Vendor, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateChassisSerial(_In_ UINT64 Seed, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);

// SMBIOS reading functions
NTSTATUS ReadSMBIOSString(_In_ PVOID TableBase, _In_ ULONG TableSize, _In_ UCHAR Type, _In_ UCHAR StringIndex, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);

// SMBIOS patching functions
NTSTATUS FindSMBIOSTable(_Out_ PVOID* TableAddress, _Out_ PULONG TableSize);
NTSTATUS PatchSMBIOSString(_In_ PVOID TableBase, _In_ ULONG TableSize, _In_ UCHAR Type, _In_ UCHAR StringIndex, _In_ PCHAR NewValue);
NTSTATUS ApplySMBIOSChanges(_In_ PSYSTEM_PROFILE Profile);
NTSTATUS BackupSMBIOSTables(VOID);
NTSTATUS RestoreSMBIOSTables(VOID);
