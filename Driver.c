#include <ntddk.h>
#include <ntstrsafe.h>

// Include your helper modules
#include "Driver.h"
#include "ProfileManager.h"
#include "RegistryHelper.h"
#include "EpicCleaner.h"
#include "NetworkCleaner.h"
#include "XboxCleaner.h"
#include "CryptoProvider.h"  // Add explicit include

// Global state - NO synchronization needed (single-threaded IOCTL processing)
PROFILE_STATE g_ProfileState = { 0 };

//
// Forward declarations with proper SAL annotations
//
_Dispatch_type_(IRP_MJ_CREATE)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceCreate(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

_Dispatch_type_(IRP_MJ_CLOSE)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS DeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

DRIVER_UNLOAD DriverUnload;

//
// DriverEntry - Main entry point
//
_Use_decl_annotations_
NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLink;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    PAGED_CODE();

    DbgPrint("PCCleanup: ===============================================\n");
    DbgPrint("PCCleanup: DriverEntry START (WDM Mode)\n");
    DbgPrint("PCCleanup: ===============================================\n");

    // CRITICAL: Initialize crypto provider FIRST (before any random generation)
    // This ensures the entropy pool is ready and prevents crashes during KDU loading
    status = InitializeCryptographicProvider();
    if (!NT_SUCCESS(status)) {
        DbgPrint("PCCleanup: Crypto provider initialization FAILED - 0x%08X\n", status);
        return status;
    }
    DbgPrint("PCCleanup: Crypto provider initialized\n");

    // CRITICAL: Zero out global state SECOND
    RtlZeroMemory(&g_ProfileState, sizeof(PROFILE_STATE));
    g_ProfileState.isModified = FALSE;

    // Set driver callbacks
    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;

    // Create device object
    RtlInitUnicodeString(&deviceName, L"\\Device\\PCCleanupDriver");

    status = IoCreateDevice(
        DriverObject,
        0,                          // No device extension
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,                      // Not exclusive
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("PCCleanup: IoCreateDevice FAILED - 0x%08X\n", status);
        return status;
    }

    DbgPrint("PCCleanup: Device created successfully\n");

    // Create symbolic link for user-mode access
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\PCCleanupDriver");
    status = IoCreateSymbolicLink(&symLink, &deviceName);

    if (!NT_SUCCESS(status)) {
        DbgPrint("PCCleanup: IoCreateSymbolicLink FAILED - 0x%08X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DbgPrint("PCCleanup: Symbolic link created successfully\n");

    // Set device flags
    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("PCCleanup: ===============================================\n");
    DbgPrint("PCCleanup: Driver loaded successfully!\n");
    DbgPrint("PCCleanup: Device: \\Device\\PCCleanupDriver\n");
    DbgPrint("PCCleanup: Symlink: \\\\.\\PCCleanupDriver\n");
    DbgPrint("PCCleanup: User-mode can now connect\n");
    DbgPrint("PCCleanup: ===============================================\n");

    return STATUS_SUCCESS;
}

//
// DriverUnload - Cleanup on driver unload
//
_Use_decl_annotations_
VOID DriverUnload(
    PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symLink;

    PAGED_CODE();

    DbgPrint("PCCleanup: ===============================================\n");
    DbgPrint("PCCleanup: Driver unloading...\n");
    DbgPrint("PCCleanup: ===============================================\n");

    // Restore original values if modified
    if (g_ProfileState.isModified) {
        DbgPrint("PCCleanup: Restoring original hardware identifiers...\n");

        NTSTATUS status = RestoreRegistryValues();
        if (NT_SUCCESS(status)) {
            DbgPrint("PCCleanup: Original values restored successfully\n");
        }
        else {
            DbgPrint("PCCleanup: WARNING: Failed to restore values - 0x%08X\n", status);
        }

        g_ProfileState.isModified = FALSE;
    }
    else {
        DbgPrint("PCCleanup: No modifications to restore\n");
    }

    // Cleanup crypto provider
    CleanupCryptographicProvider();
    DbgPrint("PCCleanup: Crypto provider cleaned up\n");

    // Delete symbolic link
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\PCCleanupDriver");
    IoDeleteSymbolicLink(&symLink);
    DbgPrint("PCCleanup: Symbolic link deleted\n");

    // Delete device object
    if (DriverObject->DeviceObject != NULL) {
        IoDeleteDevice(DriverObject->DeviceObject);
        DbgPrint("PCCleanup: Device object deleted\n");
    }

    DbgPrint("PCCleanup: ===============================================\n");
    DbgPrint("PCCleanup: Driver unloaded successfully\n");
    DbgPrint("PCCleanup: ===============================================\n");
}

//
// DeviceCreate - Handle IRP_MJ_CREATE
//
_Use_decl_annotations_
NTSTATUS DeviceCreate(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    DbgPrint("PCCleanup: User-mode application connected\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

//
// DeviceClose - Handle IRP_MJ_CLOSE
//
_Use_decl_annotations_
NTSTATUS DeviceClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    DbgPrint("PCCleanup: User-mode application disconnected\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

//
// DeviceControl - Handle IRP_MJ_DEVICE_CONTROL
//
_Use_decl_annotations_
NTSTATUS DeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack;
    ULONG ioControlCode;
    PVOID systemBuffer;
    ULONG inputLength;
    ULONG outputLength;
    ULONG_PTR information = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    // Get IRP stack location
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    // Get buffer (METHOD_BUFFERED)
    systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    DbgPrint("PCCleanup: IOCTL received: 0x%08X\n", ioControlCode);

    // Dispatch IOCTL
    switch (ioControlCode) {

    case IOCTL_APPLY_TEMP_PROFILE:
    {
        DbgPrint("PCCleanup: IOCTL_APPLY_TEMP_PROFILE\n");

        // Validate buffer sizes
        if (inputLength < sizeof(SEED_DATA)) {
            DbgPrint("PCCleanup: Input buffer too small\n");
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (outputLength < sizeof(SYSTEM_PROFILE)) {
            DbgPrint("PCCleanup: Output buffer too small\n");
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        // Validate system buffer
        if (systemBuffer == NULL) {
            DbgPrint("PCCleanup: NULL system buffer!\n");
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        PSEED_DATA seed = (PSEED_DATA)systemBuffer;
        PSYSTEM_PROFILE profile = (PSYSTEM_PROFILE)systemBuffer;

        // Backup original values (first time only)
        if (!g_ProfileState.isModified) {
            DbgPrint("PCCleanup: Backing up original values...\n");
            status = BackupOriginalValues();
            if (!NT_SUCCESS(status)) {
                DbgPrint("PCCleanup: Backup FAILED - 0x%08X\n", status);
                break;
            }
            DbgPrint("PCCleanup: Backup successful\n");
        }

        // Generate new profile
        DbgPrint("PCCleanup: Generating new profile...\n");
        status = GenerateSystemProfile(seed, profile);
        if (!NT_SUCCESS(status)) {
            DbgPrint("PCCleanup: Generate FAILED - 0x%08X\n", status);
            break;
        }
        DbgPrint("PCCleanup: Generation successful\n");

        // Apply to registry
        DbgPrint("PCCleanup: Applying to registry...\n");
        status = ApplyRegistryModifications(profile);
        if (!NT_SUCCESS(status)) {
            DbgPrint("PCCleanup: Apply registry FAILED - 0x%08X\n", status);
            break;
        }
        DbgPrint("PCCleanup: Registry modifications successful\n");

        // Save to global state
        RtlCopyMemory(&g_ProfileState.temporary, profile, sizeof(SYSTEM_PROFILE));
        g_ProfileState.isModified = TRUE;

        information = sizeof(SYSTEM_PROFILE);
        DbgPrint("PCCleanup: Profile applied successfully!\n");
        break;
    }

    case IOCTL_RESTORE_PROFILE:
    {
        DbgPrint("PCCleanup: IOCTL_RESTORE_PROFILE\n");

        if (g_ProfileState.isModified) {
            DbgPrint("PCCleanup: Restoring original values...\n");
            status = RestoreRegistryValues();

            if (NT_SUCCESS(status)) {
                g_ProfileState.isModified = FALSE;
                DbgPrint("PCCleanup: Restore successful\n");
            }
            else {
                DbgPrint("PCCleanup: Restore FAILED - 0x%08X\n", status);
            }
        }
        else {
            DbgPrint("PCCleanup: No modifications to restore\n");
            status = STATUS_SUCCESS;
        }
        break;
    }

    case IOCTL_GET_PROFILE:
    {
        DbgPrint("PCCleanup: IOCTL_GET_PROFILE\n");

        if (outputLength < sizeof(SYSTEM_PROFILE)) {
            DbgPrint("PCCleanup: Output buffer too small\n");
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (systemBuffer == NULL) {
            DbgPrint("PCCleanup: NULL system buffer!\n");
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (g_ProfileState.isModified) {
            PSYSTEM_PROFILE profile = (PSYSTEM_PROFILE)systemBuffer;
            RtlCopyMemory(profile, &g_ProfileState.temporary, sizeof(SYSTEM_PROFILE));
            information = sizeof(SYSTEM_PROFILE);
            status = STATUS_SUCCESS;
            DbgPrint("PCCleanup: Returned active profile\n");
        }
        else {
            status = STATUS_DEVICE_NOT_READY;
            DbgPrint("PCCleanup: No active profile available\n");
        }
        break;
    }

    case IOCTL_GET_STATUS:
    {
        DbgPrint("PCCleanup: IOCTL_GET_STATUS - Driver operational\n");
        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_CLEAN_EPIC_TRACES:
    {
        DbgPrint("PCCleanup: IOCTL_CLEAN_EPIC_TRACES\n");

        if (outputLength >= sizeof(EPIC_CLEANUP_STATS) && systemBuffer != NULL) {
            PEPIC_CLEANUP_STATS stats = (PEPIC_CLEANUP_STATS)systemBuffer;
            status = EpicCleanerExecute(stats);
            if (NT_SUCCESS(status)) {
                information = sizeof(EPIC_CLEANUP_STATS);
                DbgPrint("PCCleanup: Epic cleanup completed\n");
            }
        }
        else {
            status = EpicCleanerExecute(NULL);
            DbgPrint("PCCleanup: Epic cleanup completed (no stats)\n");
        }
        break;
    }

    case IOCTL_CLEAN_NETWORK_TRACES:
    {
        DbgPrint("PCCleanup: IOCTL_CLEAN_NETWORK_TRACES\n");

        if (outputLength >= sizeof(NETWORK_CLEANUP_STATS) && systemBuffer != NULL) {
            PNETWORK_CLEANUP_STATS stats = (PNETWORK_CLEANUP_STATS)systemBuffer;
            status = NetworkCleanerExecute(stats);
            if (NT_SUCCESS(status)) {
                information = sizeof(NETWORK_CLEANUP_STATS);
            }
        }
        else {
            status = NetworkCleanerExecute(NULL);
        }
        break;
    }

    case IOCTL_CLEAN_XBOX_TRACES:
    {
        DbgPrint("PCCleanup: IOCTL_CLEAN_XBOX_TRACES\n");

        if (outputLength >= sizeof(XBOX_CLEANUP_STATS) && systemBuffer != NULL) {
            PXBOX_CLEANUP_STATS stats = (PXBOX_CLEANUP_STATS)systemBuffer;
            status = XboxCleanerExecute(stats);
            if (NT_SUCCESS(status)) {
                information = sizeof(XBOX_CLEANUP_STATS);
            }
        }
        else {
            status = XboxCleanerExecute(NULL);
        }
        break;
    }

    default:
    {
        DbgPrint("PCCleanup: Unknown IOCTL: 0x%08X\n", ioControlCode);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
    }

    // Complete the IRP
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("PCCleanup: IOCTL completed with status: 0x%08X\n", status);

    return status;
}
