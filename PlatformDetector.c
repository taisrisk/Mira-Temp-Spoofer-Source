#include "PlatformDetector.h"
#include "EntropyCollector.h"
#include <ntstrsafe.h>

//
// DetectHardwarePlatform - Detect CPU and motherboard vendor
//
NTSTATUS DetectHardwarePlatform(_Out_ PPLATFORM_INFO PlatformInfo)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;
    UCHAR buffer[256];
    PKEY_VALUE_PARTIAL_INFORMATION keyInfo = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;
    ULONG resultLength;

    RtlZeroMemory(PlatformInfo, sizeof(PLATFORM_INFO));

    PlatformInfo->Vendor = VENDOR_GENERIC;
    PlatformInfo->IsIntel = TRUE;

    // Detect CPU vendor
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_QUERY_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&valueName, L"VendorIdentifier");
        status = ZwQueryValueKey(hKey, &valueName, KeyValuePartialInformation, keyInfo, sizeof(buffer), &resultLength);

        if (NT_SUCCESS(status)) {
            PWCHAR vendorId = (PWCHAR)keyInfo->Data;

            if (wcsstr(vendorId, L"GenuineIntel") != NULL) {
                PlatformInfo->IsIntel = TRUE;
                RtlStringCbCopyA(PlatformInfo->ChipsetModel, sizeof(PlatformInfo->ChipsetModel), "Intel");
            }
            else if (wcsstr(vendorId, L"AuthenticAMD") != NULL) {
                PlatformInfo->IsIntel = FALSE;
                RtlStringCbCopyA(PlatformInfo->ChipsetModel, sizeof(PlatformInfo->ChipsetModel), "AMD");
            }
        }

        ZwClose(hKey);
    }

    // Detect motherboard vendor based on entropy for variety
    UINT64 entropy;
    GenerateSecureEntropy(&entropy);
    UINT32 vendorSelect = (UINT32)(entropy % 100);

    if (vendorSelect < 30) {
        PlatformInfo->Vendor = VENDOR_ASUS;
        PlatformInfo->SubsysVendorId = 0x1043;
    }
    else if (vendorSelect < 50) {
        PlatformInfo->Vendor = VENDOR_GIGABYTE;
        PlatformInfo->SubsysVendorId = 0x1458;
    }
    else if (vendorSelect < 70) {
        PlatformInfo->Vendor = VENDOR_MSI;
        PlatformInfo->SubsysVendorId = 0x1462;
    }
    else if (vendorSelect < 85) {
        PlatformInfo->Vendor = VENDOR_ASROCK;
        PlatformInfo->SubsysVendorId = 0x1849;
    }
    else {
        PlatformInfo->Vendor = VENDOR_INTEL;
        PlatformInfo->SubsysVendorId = 0x8086;
    }

    return STATUS_SUCCESS;
}

//
// EncodePlatformFlags - Encode platform information into flags
//
UINT32 EncodePlatformFlags(_In_opt_ PPLATFORM_INFO PlatformInfo)
{
    UINT32 flags = 0;
    LARGE_INTEGER systemTime;
    UINT32 year, month;

    KeQuerySystemTime(&systemTime);
    
    UINT64 timeSeconds = systemTime.QuadPart / 10000000ULL;
    UINT64 daysSince1601 = timeSeconds / 86400ULL;
    UINT64 daysSince2020 = daysSince1601 - 153722ULL;
    
    year = (UINT32)(daysSince2020 / 365) % 16;
    month = (UINT32)((daysSince2020 % 365) / 30) % 12 + 1;

    if (PlatformInfo) {
        if (PlatformInfo->IsIntel) {
            flags |= PLATFORM_CPU_INTEL;
        }

        UINT32 vendor = (UINT32)PlatformInfo->Vendor & 0xF;
        flags |= (vendor << PLATFORM_VENDOR_SHIFT) & PLATFORM_VENDOR_MASK;
    } else {
        UINT64 entropy;
        GenerateSecureEntropy(&entropy);
        
        flags |= (entropy & 1) ? PLATFORM_CPU_INTEL : 0;
        UINT32 randomVendor = (UINT32)(entropy >> 8) % 6;
        flags |= (randomVendor << PLATFORM_VENDOR_SHIFT) & PLATFORM_VENDOR_MASK;
    }

    flags |= (year << PLATFORM_YEAR_SHIFT) & PLATFORM_YEAR_MASK;
    flags |= (month << PLATFORM_MONTH_SHIFT) & PLATFORM_MONTH_MASK;

    UINT64 additionalEntropy;
    GenerateSecureEntropy(&additionalEntropy);
    UINT32 entropyBits = (UINT32)(additionalEntropy >> 32) & 0x7FFFF;
    flags |= (entropyBits << PLATFORM_ENTROPY_SHIFT) & PLATFORM_ENTROPY_MASK;

    return flags;
}

//
// DecodePlatformFlags - Decode platform information from flags
//
NTSTATUS DecodePlatformFlags(_In_ UINT32 PlatformFlags, _Out_ PPLATFORM_INFO PlatformInfo)
{
    if (!PlatformInfo) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(PlatformInfo, sizeof(PLATFORM_INFO));

    PlatformInfo->IsIntel = (PlatformFlags & PLATFORM_CPU_INTEL) ? TRUE : FALSE;

    UINT32 vendor = (PlatformFlags & PLATFORM_VENDOR_MASK) >> PLATFORM_VENDOR_SHIFT;
    PlatformInfo->Vendor = (MOTHERBOARD_VENDOR)(vendor % 6);

    if (PlatformInfo->IsIntel) {
        RtlStringCbCopyA(PlatformInfo->ChipsetModel, sizeof(PlatformInfo->ChipsetModel), "Intel");
    } else {
        RtlStringCbCopyA(PlatformInfo->ChipsetModel, sizeof(PlatformInfo->ChipsetModel), "AMD");
    }

    switch (PlatformInfo->Vendor) {
    case VENDOR_ASUS:
        PlatformInfo->SubsysVendorId = 0x1043;
        break;
    case VENDOR_GIGABYTE:
        PlatformInfo->SubsysVendorId = 0x1458;
        break;
    case VENDOR_MSI:
        PlatformInfo->SubsysVendorId = 0x1462;
        break;
    case VENDOR_ASROCK:
        PlatformInfo->SubsysVendorId = 0x1849;
        break;
    case VENDOR_INTEL:
        PlatformInfo->SubsysVendorId = 0x8086;
        break;
    default:
        PlatformInfo->SubsysVendorId = 0x0000;
        break;
    }

    return STATUS_SUCCESS;
}