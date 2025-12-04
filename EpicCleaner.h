#pragma once

#include <ntddk.h>
#include <wdf.h>

// Epic Games paths structure
typedef struct _EPIC_CLEANUP_PATHS {
    PCWSTR RelativePath;
    BOOLEAN IsRecursive;
    BOOLEAN DeleteFolder;
} EPIC_CLEANUP_PATHS, *PEPIC_CLEANUP_PATHS;

// Epic Games cleanup statistics
typedef struct _EPIC_CLEANUP_STATS {
    ULONG FilesDeleted;
    ULONG FoldersDeleted;
    ULONG FilesSkipped;
    ULONG Errors;
} EPIC_CLEANUP_STATS, *PEPIC_CLEANUP_STATS;

// Function prototypes
NTSTATUS CleanupEpicGamesTraces(_Out_opt_ PEPIC_CLEANUP_STATS Stats);
NTSTATUS DeleteDirectoryRecursive(_In_ PUNICODE_STRING DirectoryPath);
NTSTATUS DeleteFileSecure(_In_ PUNICODE_STRING FilePath);
NTSTATUS GetUserProfilePath(_Out_ PUNICODE_STRING ProfilePath);
NTSTATUS EnumerateAndDeleteFiles(_In_ PUNICODE_STRING DirectoryPath, _In_ BOOLEAN Recursive);
NTSTATUS CleanEpicRegistryTraces(VOID);

// Exported cleanup function
NTSTATUS EpicCleanerExecute(_Out_opt_ PEPIC_CLEANUP_STATS Stats);
