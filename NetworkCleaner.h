#pragma once

#include <ntddk.h>

// Network cleanup statistics
typedef struct _NETWORK_CLEANUP_STATS {
    ULONG DnsFlushes;
    ULONG TcpResets;
    ULONG ArpClears;
    ULONG NetBiosClears;
    ULONG Errors;
} NETWORK_CLEANUP_STATS, *PNETWORK_CLEANUP_STATS;

// Network cleaner functions
NTSTATUS NetworkCleanerExecute(_Out_opt_ PNETWORK_CLEANUP_STATS Stats);
NTSTATUS FlushDnsCache(VOID);
NTSTATUS ResetTcpConnections(VOID);
NTSTATUS ClearArpCache(VOID);
NTSTATUS ClearNetBiosCache(VOID);
NTSTATUS ClearDHCPLeases(VOID);
NTSTATUS ClearNetworkHistory(VOID);
NTSTATUS ClearNetworkTraces(VOID);
NTSTATUS ClearPrefetchNetworkFiles(VOID);