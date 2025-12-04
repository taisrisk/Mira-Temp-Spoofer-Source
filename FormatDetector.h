#pragma once
#include <ntddk.h>

// Detected format types
typedef enum _SERIAL_FORMAT {
    FORMAT_UNKNOWN = 0,
    FORMAT_ASUS,           // 5B1234567890
    FORMAT_MSI,            // 601-7C02-012SB2105
    FORMAT_GIGABYTE,       // SN2511034567
    FORMAT_ASROCK,         // To Be Filled By O.E.M.
    FORMAT_INTEL,          // JKF2224IR515000
    FORMAT_DELL,           // XXXXXXX
    FORMAT_HP,             // CNV1234567
    FORMAT_LENOVO,         // PXXXXXXX
    FORMAT_GENERIC         // Mixed format
} SERIAL_FORMAT;

typedef enum _DISK_FORMAT {
    DISK_FORMAT_SAMSUNG,   // S4NXXXXXXXXX
    DISK_FORMAT_WD,        // WD-WCCXXXXXXXXXXXX
    DISK_FORMAT_SEAGATE,   // XXXXXXXX
    DISK_FORMAT_NVME,      // NVME-XXXXXXXXXXXX
    DISK_FORMAT_GENERIC
} DISK_FORMAT;

// Format detection functions
SERIAL_FORMAT DetectMotherboardFormat(_In_ PCSTR CurrentSerial);
SERIAL_FORMAT DetectBIOSFormat(_In_ PCSTR CurrentSerial);
DISK_FORMAT DetectDiskFormat(_In_ PCSTR CurrentSerial);

// Format-preserving generation
NTSTATUS GenerateInFormat_Motherboard(_In_ UINT64 Seed, _In_ SERIAL_FORMAT Format, _In_opt_ PCSTR Template, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateInFormat_BIOS(_In_ UINT64 Seed, _In_ SERIAL_FORMAT Format, _In_opt_ PCSTR Template, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateInFormat_Disk(_In_ UINT64 Seed, _In_ DISK_FORMAT Format, _In_opt_ PCSTR Template, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);