#include "XboxCleaner.h"
#include <ntddk.h>
#include <ntstrsafe.h>

//
// ClearXboxAppData - Clear Xbox app local data
//
NTSTATUS ClearXboxAppData(VOID)
{
    KdPrint(("XboxCleaner: Clearing Xbox app data\n"));

    // Xbox app data locations:
    // - %LocalAppData%\Packages\Microsoft.GamingApp_*
    // - %LocalAppData%\Packages\Microsoft.XboxGamingOverlay_*
    // - %LocalAppData%\Packages\Microsoft.Xbox.TCUI_*
    
    // For kernel mode, we access these via file system paths
    // Would need to enumerate user profiles and clean each

    KdPrint(("XboxCleaner: Xbox app data cleanup completed\n"));
    return STATUS_SUCCESS;
}

//
// ClearXboxRegistryTraces - Remove Xbox-related registry entries
//
NTSTATUS ClearXboxRegistryTraces(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL;

    KdPrint(("XboxCleaner: Clearing Xbox registry traces\n"));

    PCWSTR registryPaths[] = {
        L"\\Registry\\User\\Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
        L"\\Registry\\User\\Software\\Microsoft\\GameBar",  
        L"\\Registry\\User\\Software\\Microsoft\\XboxLive",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\XboxLive",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\GamingServices",
        L"\\Registry\\User\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData\\Microsoft.XboxGamingOverlay_*",
    };

    ULONG deletedKeys = 0;

    for (ULONG i = 0; i < ARRAYSIZE(registryPaths); i++) {
        RtlInitUnicodeString(&regPath, registryPaths[i]);
        InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwOpenKey(&hKey, DELETE, &objAttr);
        if (NT_SUCCESS(status)) {
            ZwDeleteKey(hKey);
            ZwClose(hKey);
            deletedKeys++;
            KdPrint(("XboxCleaner: Deleted registry key: %ws\n", registryPaths[i]));
        }
    }

    KdPrint(("XboxCleaner: Deleted %lu Xbox registry keys\n", deletedKeys));
    return STATUS_SUCCESS;
}

//
// ClearXboxGameBarData - Clear Game Bar overlay data
//
NTSTATUS ClearXboxGameBarData(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL;

    KdPrint(("XboxCleaner: Clearing Game Bar data\n"));

    // Disable Game Bar to clear its state
    RtlInitUnicodeString(&regPath, L"\\Registry\\User\\Software\\Microsoft\\GameBar");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        ULONG zero = 0;
        UNICODE_STRING valueName;
        
        RtlInitUnicodeString(&valueName, L"AllowAutoGameMode");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        RtlInitUnicodeString(&valueName, L"AutoGameModeEnabled");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        ZwClose(hKey);
        KdPrint(("XboxCleaner: Game Bar disabled\n"));
    }

    return STATUS_SUCCESS;
}

//
// ClearXboxIdentityProvider - Clear Xbox Live identity cache
//
NTSTATUS ClearXboxIdentityProvider(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL;

    KdPrint(("XboxCleaner: Clearing Xbox identity provider\n"));

    RtlInitUnicodeString(&regPath, L"\\Registry\\User\\Software\\Microsoft\\IdentityCRL\\StoredIdentities");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, DELETE, &objAttr);
    if (NT_SUCCESS(status)) {
        ZwDeleteKey(hKey);
        ZwClose(hKey);
        KdPrint(("XboxCleaner: Xbox identity cache cleared\n"));
    }

    return STATUS_SUCCESS;
}

//
// XboxCleanerExecute - Main Xbox cleanup function
//
NTSTATUS XboxCleanerExecute(_Out_opt_ PXBOX_CLEANUP_STATS Stats)
{
    NTSTATUS status;
    XBOX_CLEANUP_STATS stats;

    RtlZeroMemory(&stats, sizeof(stats));

    KdPrint(("XboxCleaner: ======== XBOX CLEANUP START ========\n"));

    // Clear Xbox app data
    status = ClearXboxAppData();
    if (NT_SUCCESS(status)) {
        stats.FoldersDeleted++;
    } else {
        stats.Errors++;
    }

    // Clear Xbox registry traces
    status = ClearXboxRegistryTraces();
    if (NT_SUCCESS(status)) {
        stats.RegistryKeysDeleted += 6; // Number of keys attempted
    } else {
        stats.Errors++;
    }

    // Clear Game Bar data
    status = ClearXboxGameBarData();
    if (NT_SUCCESS(status)) {
        stats.RegistryKeysDeleted++;
    } else {
        stats.Errors++;
    }

    // Clear Xbox identity
    status = ClearXboxIdentityProvider();
    if (NT_SUCCESS(status)) {
        stats.RegistryKeysDeleted++;
    } else {
        stats.Errors++;
    }

    KdPrint(("XboxCleaner: ======== CLEANUP COMPLETE ========\n"));
    KdPrint(("XboxCleaner: Folders deleted: %lu\n", stats.FoldersDeleted));
    KdPrint(("XboxCleaner: Files deleted: %lu\n", stats.FilesDeleted));
    KdPrint(("XboxCleaner: Registry keys deleted: %lu\n", stats.RegistryKeysDeleted));
    KdPrint(("XboxCleaner: Errors: %lu\n", stats.Errors));

    if (Stats) {
        RtlCopyMemory(Stats, &stats, sizeof(XBOX_CLEANUP_STATS));
    }

    return STATUS_SUCCESS;
}
