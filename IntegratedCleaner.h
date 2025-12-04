#pragma once

#include <ntddk.h>
#include <wdf.h>

// Integrated cleanup statistics combining Epic, Network, and Xbox cleaners
typedef struct _INTEGRATED_CLEANUP_STATS {
    // Epic Games cleanup
    ULONG EpicFoldersDeleted;
    ULONG EpicFilesDeleted;
    ULONG EpicRegistryKeysDeleted;
    
    // Network cleanup (no MAC spoofing)
    ULONG DnsFlushes;
    ULONG ArpClears;
    ULONG NetBiosClears;
    
    // Temp/Cache cleanup
    ULONG TempFoldersDeleted;
    ULONG TempFilesDeleted;
    ULONG CacheCleared;
    
    // Xbox cleanup
    ULONG XboxFoldersDeleted;
    ULONG XboxRegistryKeysDeleted;
    
    // Overall stats
    ULONG TotalErrors;
    ULONG TotalOperations;
} INTEGRATED_CLEANUP_STATS, *PINTEGRATED_CLEANUP_STATS;

// Main integrated cleanup function
NTSTATUS IntegratedCleanerExecute(_Out_opt_ PINTEGRATED_CLEANUP_STATS Stats);

// Individual cleanup modules (called by integrated cleaner)
NTSTATUS CleanEpicGamesAndTraces(_Inout_ PINTEGRATED_CLEANUP_STATS Stats);
NTSTATUS CleanNetworkTracesOnly(_Inout_ PINTEGRATED_CLEANUP_STATS Stats);
NTSTATUS CleanXboxTraces(_Inout_ PINTEGRATED_CLEANUP_STATS Stats);
NTSTATUS CleanTempFilesAndCache(_Inout_ PINTEGRATED_CLEANUP_STATS Stats);