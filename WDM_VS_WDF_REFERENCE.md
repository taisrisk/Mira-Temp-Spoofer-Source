# WDM vs WDF Quick Reference

## Function Mapping

### Driver Initialization

| WDF | WDM |
|-----|-----|
| `WdfDriverCreate(...)` | Manual `DriverEntry()` setup |
| `WdfDeviceCreate(...)` | `IoCreateDevice(...)` |
| `WdfDeviceCreateSymbolicLink(...)` | `IoCreateSymbolicLink(...)` |
| `WdfControlDeviceInitAllocate(...)` | Not needed (use `IoCreateDevice`) |
| `WdfControlFinishInitializing(...)` | Clear `DO_DEVICE_INITIALIZING` flag |

### I/O Queue & Request Handling

| WDF | WDM |
|-----|-----|
| `WdfIoQueueCreate(...)` | Set `DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]` |
| `WdfRequestRetrieveInputBuffer(...)` | `Irp->AssociatedIrp.SystemBuffer` |
| `WdfRequestRetrieveOutputBuffer(...)` | `Irp->AssociatedIrp.SystemBuffer` (same buffer) |
| `WdfRequestCompleteWithInformation(...)` | `IoCompleteRequest(Irp, IO_NO_INCREMENT)` |
| `WDF_IO_QUEUE_CONFIG` | Not needed (manual IRP handling) |

### Synchronization

| WDF | WDM |
|-----|-----|
| `WdfWaitLockCreate(...)` | `KeInitializeSpinLock(...)` |
| `WdfWaitLockAcquire(...)` | `KeAcquireSpinLock(&lock, &oldIrql)` |
| `WdfWaitLockRelease(...)` | `KeReleaseSpinLock(&lock, oldIrql)` |
| `WDFWAITLOCK lock` | `KSPIN_LOCK lock` |

### Device Cleanup

| WDF | WDM |
|-----|-----|
| `WDF_OBJECT_ATTRIBUTES` with cleanup callback | Manual `DriverUnload` function |
| Automatic object deletion | Manual `IoDeleteDevice(...)` |
| Automatic symbolic link deletion | Manual `IoDeleteSymbolicLink(...)` |

---

## Code Examples

### Before (WDF)

```c
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDFDRIVER driver;
    
    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    return WdfDriverCreate(DriverObject, RegistryPath, 
        WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
}
```

### After (WDM)

```c
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    UNICODE_STRING deviceName, symbolicLink;
    
    // Set handlers
    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    
    // Create device
    RtlInitUnicodeString(&deviceName, L"\\Device\\PCCleanupDriver");
    status = IoCreateDevice(DriverObject, 0, &deviceName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
        FALSE, &deviceObject);
    
    if (!NT_SUCCESS(status))
        return status;
    
    // Create symbolic link
    RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\PCCleanupDriver");
    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    return STATUS_SUCCESS;
}
```

---

## IOCTL Handler

### Before (WDF)

```c
VOID PCCleanupEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode)
{
    PVOID inputBuffer, outputBuffer;
    
    WdfRequestRetrieveInputBuffer(Request, sizeof(SEED_DATA), &inputBuffer, NULL);
    WdfRequestRetrieveOutputBuffer(Request, sizeof(SYSTEM_PROFILE), &outputBuffer, NULL);
    
    // Process IOCTL...
    
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, bytesReturned);
}
```

### After (WDM)

```c
NTSTATUS DeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;  // Input AND output
    ULONG inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG_PTR bytesReturned = 0;
    NTSTATUS status;
    
    // Process IOCTL...
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}
```

---

## Spinlock Usage

### Before (WDF)

```c
WdfWaitLockAcquire(g_ProfileState.lock, NULL);

// Critical section
g_ProfileState.isModified = TRUE;

WdfWaitLockRelease(g_ProfileState.lock);
```

### After (WDM)

```c
KIRQL oldIrql;
KeAcquireSpinLock(&g_ProfileState.lock, &oldIrql);

// Critical section (IRQL = DISPATCH_LEVEL)
// WARNING: Cannot call pageable code or allocate memory!
g_ProfileState.isModified = TRUE;

KeReleaseSpinLock(&g_ProfileState.lock, oldIrql);
```

---

## Cleanup

### Before (WDF)

```c
VOID PCCleanupEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    // WDF automatically deletes device and symbolic link
}
```

### After (WDM)

```c
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symbolicLink;
    
    // Manual cleanup required
    RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\PCCleanupDriver");
    IoDeleteSymbolicLink(&symbolicLink);
    
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}
```

---

## Key Differences

### Memory Management
- **WDF:** Automatic parent-child object hierarchy
- **WDM:** Manual allocation and deallocation

### IRQL Level
- **WDF:** `WdfWaitLock` can be used at `PASSIVE_LEVEL`
- **WDM:** `KSPIN_LOCK` raises IRQL to `DISPATCH_LEVEL`

### Error Handling
- **WDF:** Automatic cleanup on failure
- **WDM:** Must manually clean up on all error paths

### Buffered I/O
- **WDF:** Separate input/output buffers
- **WDM:** Single shared buffer (`SystemBuffer`) for both

---

## Migration Checklist

- [x] Remove `#include <wdf.h>`, add `#include <ntddk.h>`
- [x] Replace `WdfDriverCreate` with manual `DriverEntry` setup
- [x] Replace `WdfDeviceCreate` with `IoCreateDevice`
- [x] Replace `WdfDeviceCreateSymbolicLink` with `IoCreateSymbolicLink`
- [x] Replace `WdfIoQueueCreate` with `MajorFunction` array
- [x] Replace `WdfWaitLock` with `KSPIN_LOCK`
- [x] Replace `WdfRequest*` APIs with IRP handling
- [x] Implement manual `DriverUnload` function
- [x] Update project settings (remove WDF libraries)
- [x] Test with KDU loader

---

## Performance Considerations

| Aspect | WDF | WDM |
|--------|-----|-----|
| **Code Size** | Larger (includes framework) | Smaller (direct API) |
| **Load Time** | Slower (framework init) | Faster (no framework) |
| **Runtime Overhead** | Moderate (abstraction layer) | Minimal (direct calls) |
| **KDU Compatible** | ? No | ? Yes |

---

## When to Use

### Use WDF When:
- Building production drivers for Windows 10+
- Need automatic PnP/Power management
- Want framework safety features
- Don't need KDU loading

### Use WDM When:
- Need KDU/kdmapper compatibility
- Building rootkits or security tools
- Minimal footprint required
- Legacy Windows support (XP/Vista)

---

Your driver is now WDM-compliant! ??
