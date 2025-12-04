#pragma once

#include <ntddk.h>

// Xbox cleanup statistics  
typedef struct _XBOX_CLEANUP_STATS {
    ULONG FoldersDeleted;
    ULONG FilesDeleted;
    ULONG RegistryKeysDeleted;
    ULONG Errors;
} XBOX_CLEANUP_STATS, *PXBOX_CLEANUP_STATS;

// Xbox cleaner functions
NTSTATUS XboxCleanerExecute(_Out_opt_ PXBOX_CLEANUP_STATS Stats);
NTSTATUS ClearXboxAppData(VOID);
NTSTATUS ClearXboxRegistryTraces(VOID);
NTSTATUS ClearXboxGameBarData(VOID);
NTSTATUS ClearXboxIdentityProvider(VOID);