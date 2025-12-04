#include "NetworkSpoofing.h"
#include "CryptoProvider.h"
#include <ntstrsafe.h>

//
// GenerateLocallyAdministeredMAC - Generate proper locally administered MAC address
//
NTSTATUS GenerateLocallyAdministeredMAC(_Out_writes_bytes_(6) PUCHAR MacAddress)
{
    NTSTATUS status;
    
    if (!MacAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    // Generate 6 random bytes
    status = GenerateCryptographicRandomBytes(MacAddress, 6);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Set locally administered bit (bit 1 of first octet) and clear multicast bit (bit 0)
    MacAddress[0] = (MacAddress[0] & 0xFE) | 0x02;

    KdPrint(("NetworkSpoofing: Generated MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        MacAddress[0], MacAddress[1], MacAddress[2],
        MacAddress[3], MacAddress[4], MacAddress[5]));

    return STATUS_SUCCESS;
}

//
// SpoofAdapterMAC - Spoof MAC address for a single adapter
//
NTSTATUS SpoofAdapterMAC(_In_ HANDLE AdapterKeyHandle, _In_ PUCHAR NewMacAddress)
{
    NTSTATUS status;
    WCHAR macWide[18];
    UNICODE_STRING valueName;
    ULONG administrativelySet = 1;

    if (!AdapterKeyHandle || !NewMacAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    // Format MAC as 12 hex characters (no separators)
    RtlStringCbPrintfW(macWide, sizeof(macWide), L"%02X%02X%02X%02X%02X%02X",
        NewMacAddress[0], NewMacAddress[1], NewMacAddress[2],
        NewMacAddress[3], NewMacAddress[4], NewMacAddress[5]);

    // Set NetworkAddress (current/active MAC)
    RtlInitUnicodeString(&valueName, L"NetworkAddress");
    status = ZwSetValueKey(AdapterKeyHandle, &valueName, 0, REG_SZ,
        macWide, (ULONG)(wcslen(macWide) * sizeof(WCHAR) + sizeof(WCHAR)));
    
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Set PermanentAddress (if exists, spoof it too for anti-cheat evasion)
    RtlInitUnicodeString(&valueName, L"PermanentAddress");
    ZwSetValueKey(AdapterKeyHandle, &valueName, 0, REG_SZ,
        macWide, (ULONG)(wcslen(macWide) * sizeof(WCHAR) + sizeof(WCHAR)));

    // Set AdministrativelySet flag
    RtlInitUnicodeString(&valueName, L"AdministrativelySet");
    ZwSetValueKey(AdapterKeyHandle, &valueName, 0, REG_DWORD,
        &administrativelySet, sizeof(administrativelySet));

    return STATUS_SUCCESS;
}

//
// EnumerateAndSpoofNetworkAdapters - Spoof MAC addresses for all network adapters
//
NTSTATUS EnumerateAndSpoofNetworkAdapters(_In_opt_ PUCHAR NewMacAddress)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, subKeyString, valueName;
    HANDLE hKey = NULL, hAdapterKey = NULL;
    ULONG index = 0;
    ULONG successCount = 0;
    ULONG attemptCount = 0;
    ULONG ethernetCount = 0;
    ULONG wifiCount = 0;
    ULONG tapCount = 0;
    UCHAR generatedMAC[6];
    BOOLEAN useProvidedMAC = (NewMacAddress != NULL);

    // If no MAC provided, generate one
    if (!useProvidedMAC) {
        status = GenerateLocallyAdministeredMAC(generatedMAC);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        NewMacAddress = generatedMAC;
    }

    // Verify MAC is locally administered
    if ((NewMacAddress[0] & 0x02) == 0) {
        KdPrint(("NetworkSpoofing: WARNING - MAC is not locally administered, fixing...\n"));
        NewMacAddress[0] = (NewMacAddress[0] & 0xFE) | 0x02;
    }

    KdPrint(("NetworkSpoofing: Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        NewMacAddress[0], NewMacAddress[1], NewMacAddress[2],
        NewMacAddress[3], NewMacAddress[4], NewMacAddress[5]));

    RtlInitUnicodeString(&regPath,
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}");

    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_ENUMERATE_SUB_KEYS | KEY_SET_VALUE, &objAttr);
    if (!NT_SUCCESS(status)) {
        KdPrint(("NetworkSpoofing: Failed to open network adapter key - 0x%08X\n", status));
        return status;
    }

    KdPrint(("NetworkSpoofing: Enumerating network adapters...\n"));

    // Enumerate adapters (0000-00FF for comprehensive coverage)
    for (index = 0; index < 256; index++) {
        WCHAR subKeyName[16];
        UCHAR descBuffer[512];
        PKEY_VALUE_PARTIAL_INFORMATION valueInfo;
        ULONG resultLength;

        // Format adapter index as 4-digit string
        RtlStringCbPrintfW(subKeyName, sizeof(subKeyName), L"%04d", index);
        RtlInitUnicodeString(&subKeyString, subKeyName);

        InitializeObjectAttributes(&objAttr, &subKeyString, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hKey, NULL);

        status = ZwOpenKey(&hAdapterKey, KEY_SET_VALUE | KEY_QUERY_VALUE, &objAttr);
        if (NT_SUCCESS(status)) {
            // Read DriverDesc to identify adapter type
            RtlInitUnicodeString(&valueName, L"DriverDesc");
            valueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)descBuffer;
            
            status = ZwQueryValueKey(hAdapterKey, &valueName, KeyValuePartialInformation,
                valueInfo, sizeof(descBuffer), &resultLength);

            if (NT_SUCCESS(status) && valueInfo->DataLength > 0) {
                PWCHAR driverDesc = (PWCHAR)valueInfo->Data;
                BOOLEAN shouldSpoof = FALSE;
                
                // Check if this is a physical or virtual adapter we want to spoof
                // Ethernet, Wi-Fi, TAP/VPN adapters
                if (wcsstr(driverDesc, L"Ethernet") || 
                    wcsstr(driverDesc, L"ethernet") ||
                    wcsstr(driverDesc, L"Wi-Fi") ||
                    wcsstr(driverDesc, L"Wireless") ||
                    wcsstr(driverDesc, L"TAP") ||
                    wcsstr(driverDesc, L"VPN") ||
                    wcsstr(driverDesc, L"Network") ||
                    wcsstr(driverDesc, L"LAN") ||
                    wcsstr(driverDesc, L"WLAN")) {
                    
                    shouldSpoof = TRUE;
                    attemptCount++;
                    
                    // Track adapter types
                    if (wcsstr(driverDesc, L"Ethernet") || wcsstr(driverDesc, L"ethernet")) {
                        ethernetCount++;
                    }
                    if (wcsstr(driverDesc, L"Wi-Fi") || wcsstr(driverDesc, L"Wireless") || wcsstr(driverDesc, L"WLAN")) {
                        wifiCount++;
                    }
                    if (wcsstr(driverDesc, L"TAP") || wcsstr(driverDesc, L"VPN")) {
                        tapCount++;
                    }
                    
                    KdPrint(("NetworkSpoofing: Found adapter %ws: %ws\n", subKeyName, driverDesc));
                }

                if (shouldSpoof) {
                    // Spoof the MAC
                    status = SpoofAdapterMAC(hAdapterKey, NewMacAddress);
                    if (NT_SUCCESS(status)) {
                        successCount++;
                        KdPrint(("NetworkSpoofing: SPOOFED %ws - %02X:%02X:%02X:%02X:%02X:%02X\n",
                            subKeyName,
                            NewMacAddress[0], NewMacAddress[1], NewMacAddress[2],
                            NewMacAddress[3], NewMacAddress[4], NewMacAddress[5]));
                    }
                    else {
                        KdPrint(("NetworkSpoofing: FAILED to spoof %ws - 0x%08X\n", subKeyName, status));
                    }
                }
            }

            ZwClose(hAdapterKey);
        }
    }

    ZwClose(hKey);

    KdPrint(("NetworkSpoofing: ======== SPOOFING SUMMARY ========\n"));
    KdPrint(("NetworkSpoofing: Adapters found: %d\n", attemptCount));
    KdPrint(("NetworkSpoofing: Successfully spoofed: %d\n", successCount));
    KdPrint(("NetworkSpoofing: Ethernet: %d, Wi-Fi: %d, TAP/VPN: %d\n", 
        ethernetCount, wifiCount, tapCount));
    KdPrint(("NetworkSpoofing: ==============================\n"));

    if (successCount == 0 && attemptCount > 0) {
        KdPrint(("NetworkSpoofing: WARNING - Found adapters but failed to spoof any!\n"));
        return STATUS_UNSUCCESSFUL;
    }

    return (successCount > 0) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

//
// RestoreAdapterMAC - Restore original hardware MAC
//
NTSTATUS RestoreAdapterMAC(_In_ HANDLE AdapterKeyHandle)
{
    UNICODE_STRING valueName;

    if (!AdapterKeyHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    // Delete NetworkAddress to restore hardware MAC
    RtlInitUnicodeString(&valueName, L"NetworkAddress");
    ZwDeleteValueKey(AdapterKeyHandle, &valueName);

    // Delete PermanentAddress override
    RtlInitUnicodeString(&valueName, L"PermanentAddress");
    ZwDeleteValueKey(AdapterKeyHandle, &valueName);

    // Delete AdministrativelySet flag
    RtlInitUnicodeString(&valueName, L"AdministrativelySet");
    ZwDeleteValueKey(AdapterKeyHandle, &valueName);

    return STATUS_SUCCESS;
}

//
// RestoreAllNetworkAdapters - Restore all adapters to hardware MAC
//
NTSTATUS RestoreAllNetworkAdapters(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, subKeyString;
    HANDLE hKey = NULL, hAdapterKey = NULL;
    ULONG index = 0;
    ULONG restoredCount = 0;

    RtlInitUnicodeString(&regPath,
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}");

    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_ENUMERATE_SUB_KEYS, &objAttr);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (index = 0; index < 256; index++) {
        WCHAR subKeyName[16];

        RtlStringCbPrintfW(subKeyName, sizeof(subKeyName), L"%04d", index);
        RtlInitUnicodeString(&subKeyString, subKeyName);

        InitializeObjectAttributes(&objAttr, &subKeyString, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hKey, NULL);

        status = ZwOpenKey(&hAdapterKey, KEY_SET_VALUE, &objAttr);
        if (NT_SUCCESS(status)) {
            status = RestoreAdapterMAC(hAdapterKey);
            if (NT_SUCCESS(status)) {
                restoredCount++;
            }
            ZwClose(hAdapterKey);
        }
    }

    ZwClose(hKey);

    KdPrint(("NetworkSpoofing: Restored %d network adapters to hardware MAC\n", restoredCount));
    return STATUS_SUCCESS;
}
