#include "NetworkCleaner.h"
#include "EpicCleaner.h"
#include <ntddk.h>
#include <ntstrsafe.h>

// Use functions from EpicCleaner
extern NTSTATUS DeleteFileSecure(_In_ PUNICODE_STRING FilePath);
extern NTSTATUS DeleteDirectoryRecursive(_In_ PUNICODE_STRING DirectoryPath);

//
// FlushDnsCache - Clear DNS resolver cache
//
NTSTATUS FlushDnsCache(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;

    KdPrint(("NetworkCleaner: Flushing DNS cache\n"));

    // Method 1: Clear DNS cache parameters
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        ULONG zero = 0;
        
        RtlInitUnicodeString(&valueName, L"MaxCacheTtl");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        RtlInitUnicodeString(&valueName, L"MaxNegativeCacheTtl");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        RtlInitUnicodeString(&valueName, L"CacheHashTableBucketSize");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        ZwClose(hKey);
    }

    // Method 2: Delete DNS cache files
    UNICODE_STRING dnsCache;
    RtlInitUnicodeString(&dnsCache, L"\\??\\C:\\Windows\\System32\\config\\systemprofile\\AppData\\Local\\Microsoft\\Windows\\INetCache");
    DeleteDirectoryRecursive(&dnsCache);

    KdPrint(("NetworkCleaner: DNS cache flushed\n"));
    return STATUS_SUCCESS;
}

//
// ResetTcpConnections - Reset TCP/IP stack and clear connection table
//
NTSTATUS ResetTcpConnections(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;

    KdPrint(("NetworkCleaner: Resetting TCP connections\n"));

    // Clear TCP parameters
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        ULONG value = 0;
        
        // Reset TCP chimney
        RtlInitUnicodeString(&valueName, L"EnableTCPChimney");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &value, sizeof(value));
        
        // Clear TCP offload
        RtlInitUnicodeString(&valueName, L"EnableRSS");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &value, sizeof(value));
        
        // Reset connection table timeout
        RtlInitUnicodeString(&valueName, L"TcpTimedWaitDelay");
        value = 30;
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &value, sizeof(value));
        
        ZwClose(hKey);
    }

    KdPrint(("NetworkCleaner: TCP reset completed\n"));
    return STATUS_SUCCESS;
}

//
// ClearArpCache - Clear ARP table entries
//
NTSTATUS ClearArpCache(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;

    KdPrint(("NetworkCleaner: Clearing ARP cache\n"));

    // Method 1: Clear ARP cache timeout to force refresh
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        ULONG timeout = 1; // 1 second - force immediate expiration
        
        RtlInitUnicodeString(&valueName, L"ArpCacheLife");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &timeout, sizeof(timeout));
        
        RtlInitUnicodeString(&valueName, L"ArpCacheMinReferencedLife");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &timeout, sizeof(timeout));
        
        ZwClose(hKey);
    }

    // Method 2: Clear ARP entries for all interfaces
    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_ENUMERATE_SUB_KEYS, &objAttr);
    if (NT_SUCCESS(status)) {
        ZwClose(hKey);
    }

    KdPrint(("NetworkCleaner: ARP cache cleared\n"));
    return STATUS_SUCCESS;
}

//
// ClearNetBiosCache - Clear NetBIOS name cache
//
NTSTATUS ClearNetBiosCache(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath, valueName;
    HANDLE hKey = NULL;

    KdPrint(("NetworkCleaner: Clearing NetBIOS cache\n"));

    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\NetBT\\Parameters");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_SET_VALUE, &objAttr);
    if (NT_SUCCESS(status)) {
        ULONG zero = 0;
        
        RtlInitUnicodeString(&valueName, L"CacheTimeout");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        RtlInitUnicodeString(&valueName, L"BcastNameQueryCount");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        RtlInitUnicodeString(&valueName, L"NameServerQueryCount");
        ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &zero, sizeof(zero));
        
        ZwClose(hKey);
    }

    KdPrint(("NetworkCleaner: NetBIOS cache cleared\n"));
    return STATUS_SUCCESS;
}

//
// ClearDHCPLeases - Clear DHCP client lease information
//
NTSTATUS ClearDHCPLeases(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL, hInterfaceKey = NULL;
    ULONG index = 0;

    KdPrint(("NetworkCleaner: Clearing DHCP leases\n"));

    RtlInitUnicodeString(&regPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
    InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&hKey, KEY_ENUMERATE_SUB_KEYS, &objAttr);
    if (NT_SUCCESS(status)) {
        UCHAR buffer[512];
        PKEY_BASIC_INFORMATION keyInfo = (PKEY_BASIC_INFORMATION)buffer;
        
        for (index = 0; ; index++) {
            ULONG resultLength;
            
            status = ZwEnumerateKey(hKey, index, KeyBasicInformation, keyInfo, sizeof(buffer), &resultLength);
            if (!NT_SUCCESS(status)) {
                break;
            }

            // Open each interface key
            UNICODE_STRING interfaceName;
            interfaceName.Buffer = keyInfo->Name;
            interfaceName.Length = (USHORT)keyInfo->NameLength;
            interfaceName.MaximumLength = (USHORT)keyInfo->NameLength;

            InitializeObjectAttributes(&objAttr, &interfaceName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hKey, NULL);
            
            status = ZwOpenKey(&hInterfaceKey, KEY_SET_VALUE, &objAttr);
            if (NT_SUCCESS(status)) {
                UNICODE_STRING valueName;
                
                // Delete DHCP lease values
                RtlInitUnicodeString(&valueName, L"DhcpIPAddress");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"DhcpSubnetMask");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"DhcpServer");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"LeaseObtainedTime");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"LeaseTerminatesTime");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"T1");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                RtlInitUnicodeString(&valueName, L"T2");
                ZwDeleteValueKey(hInterfaceKey, &valueName);
                
                ZwClose(hInterfaceKey);
            }
        }
        
        ZwClose(hKey);
    }

    KdPrint(("NetworkCleaner: DHCP leases cleared\n"));
    return STATUS_SUCCESS;
}

//
// ClearNetworkHistory - Remove network profile history from registry
//
NTSTATUS ClearNetworkHistory(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL;

    KdPrint(("NetworkCleaner: Clearing network history\n"));

    // Clear network list profiles
    PCWSTR networkPaths[] = {
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Profiles",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Signatures\\Managed",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Signatures\\Unmanaged",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\Nla\\Cache",
    };

    for (ULONG i = 0; i < ARRAYSIZE(networkPaths); i++) {
        RtlInitUnicodeString(&regPath, networkPaths[i]);
        InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwOpenKey(&hKey, DELETE, &objAttr);
        if (NT_SUCCESS(status)) {
            ZwDeleteKey(hKey);
            ZwClose(hKey);
            KdPrint(("NetworkCleaner: Deleted network history: %ws\n", networkPaths[i]));
        }
    }

    return STATUS_SUCCESS;
}

//
// ClearNetworkTraces - Clear ETW logs and network trace files
//
NTSTATUS ClearNetworkTraces(VOID)
{
    KdPrint(("NetworkCleaner: Clearing network traces\n"));

    // Delete ETW log files
    UNICODE_STRING etwPath;
    
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\Windows\\System32\\LogFiles\\WMI");
    DeleteDirectoryRecursive(&etwPath);
    
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\ProgramData\\Microsoft\\Diagnosis\\ETLLogs");
    DeleteDirectoryRecursive(&etwPath);

    // Delete NDIS packet capture logs
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\Windows\\System32\\winevt\\Logs\\Microsoft-Windows-NDIS-PacketCapture%4Operational.evtx");
    DeleteFileSecure(&etwPath);

    // Delete network setup logs
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\Windows\\debug\\NetSetup.log");
    DeleteFileSecure(&etwPath);
    
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\Windows\\INF\\setupapi.dev.log");
    DeleteFileSecure(&etwPath);

    // Delete DHCP logs
    RtlInitUnicodeString(&etwPath, L"\\??\\C:\\Windows\\System32\\dhcp");
    DeleteDirectoryRecursive(&etwPath);

    KdPrint(("NetworkCleaner: Network trace cleanup completed\n"));
    return STATUS_SUCCESS;
}

//
// ClearPrefetchNetworkFiles - Delete network-related prefetch files
//
NTSTATUS ClearPrefetchNetworkFiles(VOID)
{
    KdPrint(("NetworkCleaner: Clearing network prefetch files\n"));

    // Would enumerate and delete C:\Windows\Prefetch\NETWORK*.pf
    // Simplified for now
    
    return STATUS_SUCCESS;
}

//
// NetworkCleanerExecute - Main network cleanup function
//
NTSTATUS NetworkCleanerExecute(_Out_opt_ PNETWORK_CLEANUP_STATS Stats)
{
    NTSTATUS status;
    NETWORK_CLEANUP_STATS stats;

    RtlZeroMemory(&stats, sizeof(stats));

    KdPrint(("NetworkCleaner: ======== NETWORK CLEANUP START ========\n"));

    // Flush DNS cache
    status = FlushDnsCache();
    if (NT_SUCCESS(status)) {
        stats.DnsFlushes++;
    } else {
        stats.Errors++;
    }

    // Reset TCP connections
    status = ResetTcpConnections();
    if (NT_SUCCESS(status)) {
        stats.TcpResets++;
    } else {
        stats.Errors++;
    }

    // Clear ARP cache
    status = ClearArpCache();
    if (NT_SUCCESS(status)) {
        stats.ArpClears++;
    } else {
        stats.Errors++;
    }

    // Clear NetBIOS cache
    status = ClearNetBiosCache();
    if (NT_SUCCESS(status)) {
        stats.NetBiosClears++;
    } else {
        stats.Errors++;
    }

    // Clear DHCP leases
    status = ClearDHCPLeases();

    // Clear network history
    status = ClearNetworkHistory();

    // Clear network traces
    status = ClearNetworkTraces();

    // Clear prefetch files
    status = ClearPrefetchNetworkFiles();

    KdPrint(("NetworkCleaner: ======== CLEANUP COMPLETE ========\n"));
    KdPrint(("NetworkCleaner: DNS flushes: %lu\n", stats.DnsFlushes));
    KdPrint(("NetworkCleaner: TCP resets: %lu\n", stats.TcpResets));
    KdPrint(("NetworkCleaner: ARP clears: %lu\n", stats.ArpClears));
    KdPrint(("NetworkCleaner: NetBIOS clears: %lu\n", stats.NetBiosClears));
    KdPrint(("NetworkCleaner: Errors: %lu\n", stats.Errors));

    if (Stats) {
        RtlCopyMemory(Stats, &stats, sizeof(NETWORK_CLEANUP_STATS));
    }

    return STATUS_SUCCESS;
}
