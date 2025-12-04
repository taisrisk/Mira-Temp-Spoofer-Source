#pragma once

#include <ntddk.h>

// Motherboard manufacturer IDs
typedef enum _MOTHERBOARD_VENDOR {
    VENDOR_ASUS = 0,
    VENDOR_MSI = 1,
    VENDOR_GIGABYTE = 2,
    VENDOR_ASROCK = 3,
    VENDOR_INTEL = 4,
    VENDOR_GENERIC = 5
} MOTHERBOARD_VENDOR;

// Platform detection structure
typedef struct _PLATFORM_INFO {
    BOOLEAN IsIntel;
    MOTHERBOARD_VENDOR Vendor;
    CHAR ChipsetModel[32];
    USHORT SubsysVendorId;
} PLATFORM_INFO, * PPLATFORM_INFO;

// Platform flags encoding (32 bits)
#define PLATFORM_CPU_INTEL          0x00000001
#define PLATFORM_VENDOR_MASK        0x0000001E
#define PLATFORM_YEAR_MASK          0x000001E0
#define PLATFORM_MONTH_MASK         0x00001E00
#define PLATFORM_ENTROPY_MASK       0xFFFFE000

// Shift positions for platform flags
#define PLATFORM_VENDOR_SHIFT       1
#define PLATFORM_YEAR_SHIFT         5
#define PLATFORM_MONTH_SHIFT        9
#define PLATFORM_ENTROPY_SHIFT      13

// Platform detection and encoding functions
NTSTATUS DetectHardwarePlatform(_Out_ PPLATFORM_INFO PlatformInfo);
UINT32 EncodePlatformFlags(_In_opt_ PPLATFORM_INFO PlatformInfo);
NTSTATUS DecodePlatformFlags(_In_ UINT32 PlatformFlags, _Out_ PPLATFORM_INFO PlatformInfo);