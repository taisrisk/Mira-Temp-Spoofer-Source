#pragma once

#include <ntddk.h>
#include "Driver.h"

// Registry helper functions
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