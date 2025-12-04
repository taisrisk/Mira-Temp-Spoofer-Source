#pragma once

#include <ntddk.h>

// Forward declarations
typedef struct _ENHANCED_SEED_DATA ENHANCED_SEED_DATA, *PENHANCED_SEED_DATA;

// IOCTL codes matching user-mode application
#define IOCTL_APPLY_TEMP_PROFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESTORE_PROFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CLEAN_EPIC_TRACES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GENERATE_SEED CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CLEAN_NETWORK_TRACES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CLEAN_XBOX_TRACES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PROFILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESTART_ADAPTERS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INTEGRATED_CLEANUP CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Registry paths for temporary modification
#define REG_CRYPTOGRAPHY L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Cryptography"
#define REG_HWPROFILE L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001"
#define REG_CURRENTVERSION L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"

// Additional registry paths for SMBIOS/Hardware identifiers
#define REG_BIOS L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\BIOS"
#define REG_SYSTEMBIOS L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\SystemInformation"
#define REG_CENTRALPROCESSOR L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"
#define REG_COMPUTERHARDWAREIDS L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\SystemInformation"

// Structures matching user-mode - Legacy seed structure (maintain compatibility)
typedef struct _SEED_DATA {
    UINT64 seedValue;
    UINT64 timestamp;
} SEED_DATA, * PSEED_DATA;

typedef struct _SYSTEM_PROFILE {
    CHAR motherboardSerial[64];
    CHAR diskSerial[64];
    CHAR machineGuid[64];
    CHAR hwProfileGuid[64];
    CHAR productId[64];
    CHAR volumeGuid[64];
    UCHAR macAddress[6];
    CHAR systemUUID[37];
    CHAR biosSerial[64];
    CHAR chassisSerial[64];
} SYSTEM_PROFILE, * PSYSTEM_PROFILE;

// Global state management - WDM version (NO spinlock - registry ops are PASSIVE_LEVEL)
typedef struct _PROFILE_STATE {
    BOOLEAN isModified;
    SYSTEM_PROFILE original;
    SYSTEM_PROFILE temporary;
    // Removed KSPIN_LOCK - not needed for PASSIVE_LEVEL operations
} PROFILE_STATE, * PPROFILE_STATE;

// Extern globals
extern PROFILE_STATE g_ProfileState;

// Function prototypes - Profile management
NTSTATUS GenerateSystemProfile(_In_ PSEED_DATA seed, _Out_ PSYSTEM_PROFILE profile);
NTSTATUS GenerateSystemProfileEnhanced(_In_ PENHANCED_SEED_DATA enhancedSeed, _Out_ PSYSTEM_PROFILE profile);
NTSTATUS ApplyRegistryModifications(_In_ PSYSTEM_PROFILE profile);
NTSTATUS RestoreRegistryValues(VOID);
NTSTATUS BackupOriginalValues(VOID);
NTSTATUS EnumerateAndSpoofNetworkAdapters(_In_opt_ PUCHAR NewMacAddress);
NTSTATUS RestoreSMBIOSTables(VOID);
NTSTATUS RestoreAllNetworkAdapters(VOID);
NTSTATUS ApplySMBIOSChanges(_In_ PSYSTEM_PROFILE profile);

// Function prototypes - GUID/UUID generation
VOID GenerateGUID(_In_ UINT64 seed, _Out_ PCHAR guidString, _In_ SIZE_T bufferSize);
VOID GenerateUUID(_In_ UINT64 seed, _Out_ PCHAR uuidString, _In_ SIZE_T bufferSize);
NTSTATUS GenerateProductId(_In_ UINT64 Seed, _Out_ PCHAR Buffer, _In_ SIZE_T BufferSize);
NTSTATUS GenerateMACAddress(_In_ UINT64 Seed, _Out_writes_bytes_(6) PUCHAR MacAddress);

// Function prototypes - Hash and entropy functions
UINT64 SimpleHash(_In_ UINT64 value);
UINT64 EnhancedHash(_In_ UINT64 value);
UINT64 HashValue(_In_ UINT64 Value);
NTSTATUS GenerateSecureRandom(_Out_ PVOID Buffer, _In_ ULONG BufferSize);

// Cryptographic provider
NTSTATUS InitializeCryptographicProvider(VOID);
VOID CleanupCryptographicProvider(VOID);

// Registry helpers
NTSTATUS SetRegistryValue(
    _In_ PCWSTR RegistryPath,
    _In_ PCWSTR ValueName,
    _In_ ULONG Type,
    _In_ PVOID Data,
    _In_ ULONG DataSize
);

NTSTATUS QueryRegistryValue(
    _In_ PCWSTR RegistryPath,
    _In_ PCWSTR ValueName,
    _Out_ PVOID Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ PULONG ResultLength
);

// WDM driver entry points
DRIVER_INITIALIZE DriverEntry;

DRIVER_UNLOAD DriverUnload;

_Dispatch_type_(IRP_MJ_CREATE)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceCreate(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

_Dispatch_type_(IRP_MJ_CLOSE)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
