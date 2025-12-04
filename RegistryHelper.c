#include "RegistryHelper.h"
#include <ntstrsafe.h>

//
// SetRegistryValue - Set a registry value
//
NTSTATUS SetRegistryValue(
    _In_ PCWSTR RegistryPath,
    _In_ PCWSTR ValueName,
    _In_ ULONG Type,
    _In_ PVOID Data,
    _In_ ULONG DataSize
)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;

    RtlInitUnicodeString(&regPath, RegistryPath);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE | KEY_ALL_ACCESS, &objAttr);
    if (!NT_SUCCESS(status)) {
        KdPrint(("RegistryHelper: Failed to open registry key %ws - 0x%x\n", RegistryPath, status));
        return status;
    }

    RtlInitUnicodeString(&valueName, ValueName);
    status = ZwSetValueKey(hKey, &valueName, 0, Type, Data, DataSize);
    
    if (NT_SUCCESS(status)) {
        KdPrint(("RegistryHelper: Successfully set %ws\\%ws\n", RegistryPath, ValueName));
    } else {
        KdPrint(("RegistryHelper: Failed to set %ws\\%ws - 0x%x\n", RegistryPath, ValueName, status));
    }

    ZwClose(hKey);
    return status;
}

//
// QueryRegistryValue - Query a registry value
//
NTSTATUS QueryRegistryValue(
    _In_ PCWSTR RegistryPath,
    _In_ PCWSTR ValueName,
    _Out_ PVOID Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ PULONG ResultLength
)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;
    UCHAR buffer[512];
    PKEY_VALUE_PARTIAL_INFORMATION keyInfo = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;
    ULONG resultLength = 0;

    if (Buffer && BufferSize > 0) {
        RtlZeroMemory(Buffer, BufferSize);
    }

    if (ResultLength) {
        *ResultLength = 0;
    }

    if (!Buffer || BufferSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitUnicodeString(&regPath, RegistryPath);
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_QUERY_VALUE, &objAttr);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlInitUnicodeString(&valueName, ValueName);
    status = ZwQueryValueKey(hKey, &valueName, KeyValuePartialInformation, keyInfo, sizeof(buffer), &resultLength);

    if (NT_SUCCESS(status) && keyInfo->DataLength <= BufferSize) {
        RtlCopyMemory(Buffer, keyInfo->Data, keyInfo->DataLength);
        if (ResultLength) {
            *ResultLength = keyInfo->DataLength;
        }
    }

    ZwClose(hKey);
    return status;
}