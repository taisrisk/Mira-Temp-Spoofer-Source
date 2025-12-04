#pragma once

#include <ntddk.h>

// Network adapter spoofing functions
NTSTATUS EnumerateAndSpoofNetworkAdapters(_In_opt_ PUCHAR NewMacAddress);
NTSTATUS GenerateLocallyAdministeredMAC(_Out_writes_bytes_(6) PUCHAR MacAddress);
NTSTATUS SpoofAdapterMAC(_In_ HANDLE AdapterKeyHandle, _In_ PUCHAR NewMacAddress);
NTSTATUS RestoreAdapterMAC(_In_ HANDLE AdapterKeyHandle);
NTSTATUS RestoreAllNetworkAdapters(VOID);
