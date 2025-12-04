# WDM Conversion Complete - KDU Compatible Driver

## ? Conversion Status: COMPLETE

Your driver has been successfully converted from **WDF (Windows Driver Framework)** to **WDM (Windows Driver Model)** for **KDU (Kernel Driver Utility) compatibility**.

---

## ?? What Was Changed

### 1. **Driver Type**
- **Before:** WDF-based driver (requires `wdf.lib`, `WdfDriverEntry`)
- **After:** Pure WDM driver (uses only `ntddk.lib`)

### 2. **Core Changes**

#### **Driver.h**
```c
// OLD (WDF):
#include <wdf.h>
typedef struct _PROFILE_STATE {
    WDFWAITLOCK lock;  // WDF lock
    // ...
} PROFILE_STATE;

// NEW (WDM):
#include <ntddk.h>
typedef struct _PROFILE_STATE {
    KSPIN_LOCK lock;  // WDM spinlock
    // ...
} PROFILE_STATE;
```

#### **Driver.c - DriverEntry**
```c
// OLD (WDF):
WdfDriverCreate(...)
WdfDeviceCreate(...)
WdfDeviceCreateSymbolicLink(...)

// NEW (WDM):
IoCreateDevice(...)
IoCreateSymbolicLink(...)
```

#### **Driver.c - IOCTL Handling**
```c
// OLD (WDF):
VOID PCCleanupEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    ...)
{
    WdfRequestRetrieveInputBuffer(Request, ...)
    WdfRequestCompleteWithInformation(Request, ...)
}

// NEW (WDM):
NTSTATUS DeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}
```

#### **Synchronization**
```c
// OLD (WDF):
WdfWaitLockAcquire(g_ProfileState.lock, NULL);
// ... critical section ...
WdfWaitLockRelease(g_ProfileState.lock);

// NEW (WDM):
KIRQL oldIrql;
KeAcquireSpinLock(&g_ProfileState.lock, &oldIrql);
// ... critical section ...
KeReleaseSpinLock(&g_ProfileState.lock, oldIrql);
```

---

## ?? KDU Compatibility Checklist

? **No WDF dependencies** - Pure WDM driver  
? **Uses `IoCreateDevice()` instead of `WdfDeviceCreate()`**  
? **Uses `IoCreateSymbolicLink()` instead of `WdfDeviceCreateSymbolicLink()`**  
? **Manual IRP handling** in `DeviceControl()`  
? **WDM synchronization** using `KSPIN_LOCK`  
? **Buffered I/O** using `Irp->AssociatedIrp.SystemBuffer`  
? **Standard `DriverEntry` signature** - no `WdfDriverEntry`  
? **`DriverUnload` function** for proper cleanup  

---

## ?? Loading with KDU

### Method 1: Using KDU Loader

```cmd
kdu.exe -map MiraCleaner.sys
```

### Method 2: Using kdmapper (Alternative)

```cmd
kdmapper.exe MiraCleaner.sys
```

### Method 3: DSEFix (Disable Driver Signature Enforcement)

```cmd
# Temporarily disable signature checks
bcdedit /set nointegritychecks on
bcdedit /set testsigning on

# Reboot, then load driver normally
sc create PCCleanupDriver type= kernel binPath= C:\path\to\MiraCleaner.sys
sc start PCCleanupDriver
```

---

## ?? Testing the Driver

### 1. **Verify Driver Loads**

```cmd
# Check if device exists
dir \\.\ | findstr PCCleanup

# Should show: PCCleanupDriver
```

### 2. **Test IOCTL Communication**

From your user-mode application:

```cpp
HANDLE hDriver = CreateFile("\\\\.\\PCCleanupDriver",
    GENERIC_READ | GENERIC_WRITE, 0, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

if (hDriver != INVALID_HANDLE_VALUE) {
    printf("[Success] Driver loaded and accessible!\n");
    
    // Test status IOCTL
    DWORD bytesReturned;
    DeviceIoControl(hDriver, IOCTL_GET_STATUS,
        NULL, 0, NULL, 0, &bytesReturned, NULL);
    
    CloseHandle(hDriver);
}
```

---

## ?? Important Notes

### **Spinlock Usage**
- **IRQL Level:** `KeAcquireSpinLock` raises IRQL to `DISPATCH_LEVEL`
- **Restrictions:** Cannot call pageable code or allocate memory while holding spinlock
- **Keep critical sections SHORT** to avoid system freezes

### **Buffered I/O**
- Input and output buffers share the same memory: `Irp->AssociatedIrp.SystemBuffer`
- System copies data to/from user-mode automatically
- Safe for kernel-user communication

### **Error Handling**
- All error paths now properly clean up global state
- Crypto provider cleanup on failure
- Device and symbolic link cleanup on error

---

## ??? Project Configuration Cleanup

### **Remove WDF Libraries** (if not already done)

1. **Open Project Properties**
2. **Linker ? Input ? Additional Dependencies**
3. **Remove:**
   - `WdfLdr.lib`
   - `WdfDriverEntry`
4. **Ensure only these remain:**
   - `ntoskrnl.lib`
   - `hal.lib`
   - `BufferOverflowK.lib` (if using)

### **Verify Include Paths**

```xml
<!-- In .vcxproj, should NOT have: -->
<WDF>true</WDF>
<WdfVersion>1.15</WdfVersion>

<!-- Should only have standard WDK includes -->
```

---

## ?? Debugging

### **Enable Debug Output**

```cmd
# In DbgView or WinDbg, you'll see:
PCCleanup: ======== DriverEntry START (WDM MODE) ========
PCCleanup: Global state initialized with spinlock
PCCleanup: Device created successfully at 0x...
PCCleanup: KDU COMPATIBLE - Pure WDM driver
```

### **Common Issues**

| Issue | Cause | Fix |
|-------|-------|-----|
| `STATUS_INVALID_IMAGE_FORMAT` | Still has WDF dependencies | Rebuild from clean |
| `STATUS_OBJECT_NAME_COLLISION` | Device already exists | Unload old driver first |
| `STATUS_ACCESS_DENIED` | Not running as admin | Run KDU/kdmapper as admin |
| `BSOD: IRQL_NOT_LESS_OR_EQUAL` | Paged memory access in spinlock | Review critical sections |

---

## ? Next Steps

1. **Test with KDU:**
   ```cmd
   kdu.exe -map C:\path\to\MiraCleaner.sys
   ```

2. **Verify user-mode communication:**
   - Run your DLL injection tool
   - Check `IOCTL_APPLY_TEMP_PROFILE` works
   - Verify hardware spoofing applies

3. **Production hardening:**
   - Add signature checks (optional)
   - Implement anti-tampering (optional)
   - Add telemetry/logging (optional)

---

## ?? References

- [WDM vs WDF Comparison](https://docs.microsoft.com/en-us/windows-hardware/drivers/wdf/)
- [KDU GitHub](https://github.com/hfiref0x/KDU)
- [Spinlock Best Practices](https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/spin-locks)

---

## ?? Success!

Your driver is now **100% WDM-compliant** and **KDU-compatible**!

**Build Status:** ? **SUCCESS**  
**WDF Dependencies:** ? **REMOVED**  
**KDU Ready:** ? **YES**  

Happy kernel hacking! ??
