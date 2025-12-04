#include "EpicCleaner.h"
#include <ntddk.h>
#include <ntstrsafe.h>

// Suppress VCR001 warning for ZwQueryDirectoryFile - function is properly declared and linked
#pragma warning(push)
#pragma warning(disable: 4996) // VCR001

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryDirectoryFile(
    _In_ HANDLE FileHandle,
    _In_opt_ HANDLE Event,
    _In_opt_ PIO_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcContext,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _Out_writes_bytes_(Length) PVOID FileInformation,
    _In_ ULONG Length,
    _In_ FILE_INFORMATION_CLASS FileInformationClass,
    _In_ BOOLEAN ReturnSingleEntry,
    _In_opt_ PUNICODE_STRING FileName,
    _In_ BOOLEAN RestartScan
);

#pragma warning(pop)

// File directory information structure
typedef struct _FILE_BOTH_DIR_INFORMATION {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    ULONG EaSize;
    CCHAR ShortNameLength;
    WCHAR ShortName[12];
    WCHAR FileName[1];
} FILE_BOTH_DIR_INFORMATION, * PFILE_BOTH_DIR_INFORMATION;

// Epic Games paths to clean (relative to user profile or system)
static EPIC_CLEANUP_PATHS g_EpicPaths[] = {
    // Launcher logs only - safe paths
    { L"\\AppData\\Local\\EpicGamesLauncher\\Saved\\Logs", TRUE, TRUE },
    { L"\\AppData\\Local\\EpicGamesLauncher\\Saved\\webcache", TRUE, TRUE },
    { L"\\AppData\\Local\\EpicGamesLauncher\\Saved\\Crashes", TRUE, TRUE },

    // Fortnite specific paths
    { L"\\AppData\\Local\\FortniteGame\\Saved\\Logs", TRUE, TRUE },
    { L"\\AppData\\Local\\FortniteGame\\Saved\\Crashes", TRUE, TRUE },

    // EasyAntiCheat traces
    { L"\\AppData\\Roaming\\EasyAntiCheat", TRUE, TRUE },
};

static PCWSTR g_SystemWidePaths[] = {
    L"\\??\\C:\\ProgramData\\Epic\\EpicGamesLauncher\\Logs",
};

//
// CheckPathExists - Safely check if path exists
//
BOOLEAN CheckPathExists(_In_ PUNICODE_STRING Path)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    HANDLE hFile = NULL;
    IO_STATUS_BLOCK ioStatus;

    if (!Path || !Path->Buffer || Path->Length == 0) {
        return FALSE;
    }

    __try {
        InitializeObjectAttributes(&objAttr, Path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&hFile, FILE_READ_ATTRIBUTES, &objAttr, &ioStatus,
            NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

        if (NT_SUCCESS(status)) {
            ZwClose(hFile);
            return TRUE;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: Exception checking path: %wZ\n", Path));
    }

    return FALSE;
}

//
// DeleteFileSecure - Securely delete a file with proper error handling
//
NTSTATUS DeleteFileSecure(_In_ PUNICODE_STRING FilePath)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    HANDLE hFile = NULL;
    IO_STATUS_BLOCK ioStatus;

    if (!FilePath || !FilePath->Buffer || FilePath->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    __try {
        InitializeObjectAttributes(&objAttr, FilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        // Try to delete without writing (safer)
        status = ZwCreateFile(&hFile, DELETE | SYNCHRONIZE, &objAttr, &ioStatus,
            NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_DELETE, FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_DELETE_ON_CLOSE, NULL, 0);

        if (NT_SUCCESS(status)) {
            ZwClose(hFile);
            return STATUS_SUCCESS;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: Exception deleting file: %wZ\n", FilePath));
        return STATUS_UNSUCCESSFUL;
    }

    return status;
}

//
// EnumerateAndDeleteFiles - Enumerate directory and delete matching files
//
NTSTATUS EnumerateAndDeleteFiles(_In_ PUNICODE_STRING DirectoryPath, _In_ BOOLEAN Recursive)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    HANDLE hDirectory = NULL;
    IO_STATUS_BLOCK ioStatus;
    PVOID buffer = NULL;
    PFILE_BOTH_DIR_INFORMATION dirInfo;
    BOOLEAN firstQuery = TRUE;
    ULONG bufferSize = 4096;

    if (!DirectoryPath || !DirectoryPath->Buffer) {
        return STATUS_INVALID_PARAMETER;
    }

    // Check if directory exists first
    if (!CheckPathExists(DirectoryPath)) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Allocate buffer safely
    buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, 'EPIC');
    if (!buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    __try {
        InitializeObjectAttributes(&objAttr, DirectoryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&hDirectory, FILE_LIST_DIRECTORY | SYNCHRONIZE, &objAttr, &ioStatus,
            NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

        if (!NT_SUCCESS(status)) {
            ExFreePoolWithTag(buffer, 'EPIC');
            return status;
        }

        while (TRUE) {
            RtlZeroMemory(buffer, bufferSize);
            
            status = ZwQueryDirectoryFile(hDirectory, NULL, NULL, NULL, &ioStatus,
                buffer, bufferSize, FileBothDirectoryInformation, FALSE, NULL, firstQuery);

            if (!NT_SUCCESS(status)) {
                if (status == STATUS_NO_MORE_FILES) {
                    status = STATUS_SUCCESS;
                }
                break;
            }

            firstQuery = FALSE;
            dirInfo = (PFILE_BOTH_DIR_INFORMATION)buffer;

            while (dirInfo && dirInfo->FileNameLength > 0) {
                // Skip . and ..
                if (dirInfo->FileName[0] == L'.' && 
                    (dirInfo->FileNameLength == sizeof(WCHAR) ||
                     (dirInfo->FileNameLength == 2 * sizeof(WCHAR) && dirInfo->FileName[1] == L'.'))) {
                    goto next_entry;
                }

                // Build full path safely
                WCHAR fullPath[512];
                SIZE_T pathLen = DirectoryPath->Length / sizeof(WCHAR);
                SIZE_T nameLen = dirInfo->FileNameLength / sizeof(WCHAR);
                
                if (pathLen + nameLen + 2 < ARRAYSIZE(fullPath)) {
                    RtlStringCbCopyW(fullPath, sizeof(fullPath), DirectoryPath->Buffer);
                    RtlStringCbCatW(fullPath, sizeof(fullPath), L"\\");
                    RtlStringCbCatNW(fullPath, sizeof(fullPath), dirInfo->FileName, dirInfo->FileNameLength);

                    UNICODE_STRING fullPathString;
                    RtlInitUnicodeString(&fullPathString, fullPath);

                    if (dirInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        if (Recursive) {
                            DeleteDirectoryRecursive(&fullPathString);
                        }
                    }
                    else {
                        DeleteFileSecure(&fullPathString);
                    }
                }

next_entry:
                if (dirInfo->NextEntryOffset == 0) {
                    break;
                }
                dirInfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)dirInfo + dirInfo->NextEntryOffset);
            }
        }

        ZwClose(hDirectory);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: Exception in EnumerateAndDeleteFiles\n"));
        status = STATUS_UNSUCCESSFUL;
        if (hDirectory) {
            ZwClose(hDirectory);
        }
    }

    ExFreePoolWithTag(buffer, 'EPIC');
    return status;
}

//
// DeleteDirectoryRecursive - Recursively delete directory with safety checks
//
NTSTATUS DeleteDirectoryRecursive(_In_ PUNICODE_STRING DirectoryPath)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    HANDLE hDirectory = NULL;
    IO_STATUS_BLOCK ioStatus;

    if (!DirectoryPath || !DirectoryPath->Buffer) {
        return STATUS_INVALID_PARAMETER;
    }

    // Check if directory exists
    if (!CheckPathExists(DirectoryPath)) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    __try {
        // First delete all contents
        status = EnumerateAndDeleteFiles(DirectoryPath, TRUE);

        // Then delete the directory itself
        InitializeObjectAttributes(&objAttr, DirectoryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&hDirectory, DELETE | SYNCHRONIZE, &objAttr, &ioStatus,
            NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_DELETE, FILE_OPEN,
            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_DELETE_ON_CLOSE, NULL, 0);

        if (NT_SUCCESS(status)) {
            ZwClose(hDirectory);
            KdPrint(("EpicCleaner: Deleted directory: %wZ\n", DirectoryPath));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: Exception deleting directory: %wZ\n", DirectoryPath));
        status = STATUS_UNSUCCESSFUL;
    }

    return status;
}

//
// CleanEpicRegistryTraces - Clean Epic Games registry entries safely
//
NTSTATUS CleanEpicRegistryTraces(VOID)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING regPath;
    HANDLE hKey = NULL;

    PCWSTR registryPaths[] = {
        // Core Epic Games paths
        L"\\Registry\\User\\Software\\Epic Games",
        L"\\Registry\\Machine\\SOFTWARE\\Epic Games",
        L"\\Registry\\Machine\\SOFTWARE\\WOW6432Node\\Epic Games",
        L"\\Registry\\User\\.DEFAULT\\SOFTWARE\\Epic Games",
        
        // COM class registrations
        L"\\Registry\\Machine\\SOFTWARE\\Classes\\com.epicgames.launcher",
        L"\\Registry\\Machine\\SOFTWARE\\Classes\\com.epicgames.fortnite",
        L"\\Registry\\User\\.DEFAULT\\SOFTWARE\\Classes\\com.epicgames.launcher",
        
        // AppContainer storage traces
        L"\\Registry\\User\\.DEFAULT\\SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Storage\\epic.launcher",
        
        // Services
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\EasyAntiCheat",
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\EpicOnlineServices",
        
        // Uninstaller traces  
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Fortnite",
        L"\\Registry\\Machine\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Fortnite",
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EpicGamesLauncher",
    };

    ULONG deletedCount = 0;

    for (ULONG i = 0; i < ARRAYSIZE(registryPaths); i++) {
        __try {
            RtlInitUnicodeString(&regPath, registryPaths[i]);
            InitializeObjectAttributes(&objAttr, &regPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

            status = ZwOpenKey(&hKey, DELETE, &objAttr);
            if (NT_SUCCESS(status)) {
                ZwDeleteKey(hKey);
                ZwClose(hKey);
                deletedCount++;
                KdPrint(("EpicCleaner: Deleted registry key: %ws\n", registryPaths[i]));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            KdPrint(("EpicCleaner: Exception cleaning registry\n"));
        }
    }

    KdPrint(("EpicCleaner: Deleted %lu Epic registry keys\n", deletedCount));
    return STATUS_SUCCESS;
}

//
// CleanupEpicGamesTraces - Main cleanup function with safety checks
//
NTSTATUS CleanupEpicGamesTraces(_Out_opt_ PEPIC_CLEANUP_STATS Stats)
{
    NTSTATUS status = STATUS_SUCCESS;
    EPIC_CLEANUP_STATS stats;
    
    RtlZeroMemory(&stats, sizeof(stats));

    KdPrint(("EpicCleaner: ======== EPIC GAMES CLEANUP START ========\n"));

    __try {
        // Clean system-wide paths
        for (ULONG i = 0; i < ARRAYSIZE(g_SystemWidePaths); i++) {
            UNICODE_STRING sysPath;
            RtlInitUnicodeString(&sysPath, g_SystemWidePaths[i]);

            if (CheckPathExists(&sysPath)) {
                status = DeleteDirectoryRecursive(&sysPath);
                if (NT_SUCCESS(status)) {
                    stats.FoldersDeleted++;
                }
            }
        }

        // Enumerate users directory
        UNICODE_STRING usersDir;
        RtlInitUnicodeString(&usersDir, L"\\??\\C:\\Users");

        if (!CheckPathExists(&usersDir)) {
            goto cleanup_registry;
        }

        OBJECT_ATTRIBUTES objAttr;
        HANDLE hUsersDir = NULL;
        IO_STATUS_BLOCK ioStatus;
        PVOID buffer = NULL;
        ULONG bufferSize = 4096;

        buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, 'EPIC');
        if (!buffer) {
            goto cleanup_registry;
        }

        InitializeObjectAttributes(&objAttr, &usersDir, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&hUsersDir, FILE_LIST_DIRECTORY | SYNCHRONIZE, &objAttr, &ioStatus,
            NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

        if (NT_SUCCESS(status)) {
            PFILE_BOTH_DIR_INFORMATION dirInfo;
            BOOLEAN firstQuery = TRUE;

            while (TRUE) {
                RtlZeroMemory(buffer, bufferSize);
                
                status = ZwQueryDirectoryFile(hUsersDir, NULL, NULL, NULL, &ioStatus,
                    buffer, bufferSize, FileBothDirectoryInformation, FALSE, NULL, firstQuery);

                if (!NT_SUCCESS(status)) {
                    break;
                }

                firstQuery = FALSE;
                dirInfo = (PFILE_BOTH_DIR_INFORMATION)buffer;

                while (dirInfo && dirInfo->FileNameLength > 0) {
                    if (dirInfo->FileName[0] != L'.' &&
                        (dirInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

                        UNICODE_STRING userName;
                        userName.Buffer = dirInfo->FileName;
                        userName.Length = (USHORT)dirInfo->FileNameLength;
                        userName.MaximumLength = (USHORT)dirInfo->FileNameLength;

                        // Skip system folders
                        UNICODE_STRING skipFolders[] = {
                            RTL_CONSTANT_STRING(L"Public"),
                            RTL_CONSTANT_STRING(L"Default"),
                            RTL_CONSTANT_STRING(L"All Users")
                        };

                        BOOLEAN skipUser = FALSE;
                        for (ULONG k = 0; k < ARRAYSIZE(skipFolders); k++) {
                            if (RtlCompareUnicodeString(&userName, &skipFolders[k], TRUE) == 0) {
                                skipUser = TRUE;
                                break;
                            }
                        }

                        if (!skipUser) {
                            WCHAR basePathBuffer[260];
                            if (userName.Length < sizeof(basePathBuffer) - 40) {
                                RtlStringCbCopyW(basePathBuffer, sizeof(basePathBuffer), L"\\??\\C:\\Users\\");
                                RtlStringCbCatNW(basePathBuffer, sizeof(basePathBuffer), userName.Buffer, userName.Length);

                                // Clean Epic paths for this user
                                for (ULONG j = 0; j < ARRAYSIZE(g_EpicPaths); j++) {
                                    WCHAR fullPath[512];
                                    RtlStringCbCopyW(fullPath, sizeof(fullPath), basePathBuffer);
                                    RtlStringCbCatW(fullPath, sizeof(fullPath), g_EpicPaths[j].RelativePath);

                                    UNICODE_STRING cleanPath;
                                    RtlInitUnicodeString(&cleanPath, fullPath);

                                    if (CheckPathExists(&cleanPath)) {
                                        status = DeleteDirectoryRecursive(&cleanPath);
                                        if (NT_SUCCESS(status)) {
                                            stats.FoldersDeleted++;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (dirInfo->NextEntryOffset == 0) {
                        break;
                    }
                    dirInfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)dirInfo + dirInfo->NextEntryOffset);
                }
            }

            ZwClose(hUsersDir);
        }

        if (buffer) {
            ExFreePoolWithTag(buffer, 'EPIC');
        }

cleanup_registry:
        // Clean registry traces
        CleanEpicRegistryTraces();

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: CRITICAL - Exception during cleanup\n"));
        stats.Errors++;
    }

    KdPrint(("EpicCleaner: ======== CLEANUP COMPLETE ========\n"));
    KdPrint(("EpicCleaner: Folders deleted: %lu\n", stats.FoldersDeleted));
    KdPrint(("EpicCleaner: Errors: %lu\n", stats.Errors));

    if (Stats) {
        RtlCopyMemory(Stats, &stats, sizeof(EPIC_CLEANUP_STATS));
    }

    return STATUS_SUCCESS;
}

//
// EpicCleanerExecute - Public entry point
//
NTSTATUS EpicCleanerExecute(_Out_opt_ PEPIC_CLEANUP_STATS Stats)
{
    KdPrint(("EpicCleaner: Execute called\n"));
    
    __try {
        return CleanupEpicGamesTraces(Stats);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KdPrint(("EpicCleaner: FATAL - Exception in execute\n"));
        return STATUS_UNSUCCESSFUL;
    }
}
