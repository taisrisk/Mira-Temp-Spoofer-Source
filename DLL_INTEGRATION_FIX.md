# DLL Integration Fixes - Critical Issues

## Issue #1: Profile Size Mismatch (CRITICAL)
```
[HWIDSpoof] Profile size mismatch: got 16, expected 555
[HWIDSpoof] Driver communication failed - profile not loaded
```

### Root Cause
The DLL is receiving only 16 bytes from `IOCTL_GET_PROFILE` instead of the full `SYSTEM_PROFILE` structure (555 bytes).

### Diagnosis Steps

1. **Check IOCTL Return Value**
   ```cpp
   // In your DLL's HWIDSpoof.cpp or similar:
   SYSTEM_PROFILE profile;
   DWORD bytesReturned = 0;
   
   BOOL success = DeviceIoControl(
       g_hDriver,
       IOCTL_GET_PROFILE,
       NULL, 0,
       &profile, sizeof(SYSTEM_PROFILE),
       &bytesReturned,
       NULL
   );
   
   printf("[Debug] DeviceIoControl returned: %d\n", success);
   printf("[Debug] Bytes returned: %lu\n", bytesReturned);
   printf("[Debug] Expected size: %zu\n", sizeof(SYSTEM_PROFILE));
   printf("[Debug] GetLastError: %lu\n", GetLastError());
   ```

2. **Verify Profile is Active**
   - The driver returns `STATUS_DEVICE_NOT_READY` if no profile is active
   - **You must call `IOCTL_APPLY_TEMP_PROFILE` BEFORE `IOCTL_GET_PROFILE`!**

### Fix #1: Ensure Profile is Applied First

In your DLL initialization:

```cpp
BOOL InitializeDriver()
{
    // 1. Open driver
    g_hDriver = CreateFile("\\\\.\\PCCleanupDriver", 
        GENERIC_READ | GENERIC_WRITE, 0, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (g_hDriver == INVALID_HANDLE_VALUE) {
        printf("[Error] Failed to open driver: %lu\n", GetLastError());
        return FALSE;
    }
    
    // 2. Apply profile FIRST (required!)
    SEED_DATA seed;
    seed.seedValue = 0x1234567890ABCDEF;  // Your seed
    seed.timestamp = GetTickCount64();
    
    SYSTEM_PROFILE profile;
    DWORD bytesReturned;
    
    printf("[Debug] Applying temporary profile...\n");
    BOOL success = DeviceIoControl(
        g_hDriver,
        IOCTL_APPLY_TEMP_PROFILE,
        &seed, sizeof(SEED_DATA),
        &profile, sizeof(SYSTEM_PROFILE),
        &bytesReturned,
        NULL
    );
    
    if (!success) {
        printf("[Error] Failed to apply profile: %lu\n", GetLastError());
        CloseHandle(g_hDriver);
        return FALSE;
    }
    
    printf("[Debug] Profile applied, bytes returned: %lu\n", bytesReturned);
    
    // 3. Now you can get the profile for hooking
    success = DeviceIoControl(
        g_hDriver,
        IOCTL_GET_PROFILE,
        NULL, 0,
        &g_SpoofedProfile, sizeof(SYSTEM_PROFILE),
        &bytesReturned,
        NULL
    );
    
    if (!success || bytesReturned != sizeof(SYSTEM_PROFILE)) {
        printf("[Error] Failed to get profile: %lu, bytes: %lu\n", 
            GetLastError(), bytesReturned);
        return FALSE;
    }
    
    printf("[Success] Profile loaded: %lu bytes\n", bytesReturned);
    return TRUE;
}
```

### Fix #2: Check Structure Sizes Match

In your DLL's header (must match driver exactly!):

```cpp
// MUST match driver's SYSTEM_PROFILE exactly!
typedef struct _SYSTEM_PROFILE {
    CHAR motherboardSerial[64];
    CHAR diskSerial[64];
    CHAR machineGuid[64];
    CHAR hwProfileGuid[64];
    CHAR productId[64];
    CHAR volumeGuid[64];
    UCHAR macAddress[6];
    CHAR systemUUID[37];
    CHAR biosSerial[64];
    CHAR chassisSerial[64];
} SYSTEM_PROFILE, *PSYSTEM_PROFILE;

// Verify size at compile time
static_assert(sizeof(SYSTEM_PROFILE) == 555, "SYSTEM_PROFILE size mismatch!");
```

### Fix #3: Add Debug Logging to Driver

Already done in your driver! The logs show:

```
PCCleanup: IOCTL_GET_PROFILE requested
PCCleanup: No active profile - call IOCTL_APPLY_TEMP_PROFILE first
```

**This confirms the issue:** Your DLL is calling `IOCTL_GET_PROFILE` without first calling `IOCTL_APPLY_TEMP_PROFILE`!

---

## Issue #2: MinHook Stub Warning

```
[MinHookStub] WARNING: Using stub implementation - hooks will not work!
[MinHookStub] Please add real MinHook library to enable hooking.
```

### Fix
See MINHOOK_FIX_GUIDE.md for complete instructions.

Quick fix:
1. Download MinHook source from https://github.com/TsudaKageyu/minhook
2. Add these files to your DLL project:
   - `minhook/src/buffer.c`
   - `minhook/src/hook.c`
   - `minhook/src/trampoline.c`
   - `minhook/src/hde/hde64.c` (for x64)
3. Remove/exclude MinHookStub.cpp
4. Rebuild

---

## Issue #3: Driver Access Denied (Error 5)

**ALREADY FIXED** in your driver (Driver.c line ~140):

```c
DECLARE_CONST_UNICODE_STRING(sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGWGX;;;WD)");
```

This allows non-admin processes to access the driver.

**WARNING:** This is for development only! For production:
```c
// Production (admin only):
DECLARE_CONST_UNICODE_STRING(sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
```

---

## Complete DLL Initialization Flow

```cpp
// 1. DLL is injected into target process
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}

// 2. Initialize driver connection
DWORD WINAPI InitThread(LPVOID lpParam)
{
    printf("[HWIDSpoof] ========== DLL LOADING ==========\n");
    
    // Open driver
    g_hDriver = CreateFile("\\\\.\\PCCleanupDriver", 
        GENERIC_READ | GENERIC_WRITE, 0, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (g_hDriver == INVALID_HANDLE_VALUE) {
        printf("[Error] Failed to open driver: %lu\n", GetLastError());
        return 1;
    }
    
    // Apply profile to driver (MUST DO THIS FIRST!)
    SEED_DATA seed = {0};
    seed.seedValue = 0x1234567890ABCDEF;
    seed.timestamp = GetTickCount64();
    
    SYSTEM_PROFILE tempProfile;
    DWORD bytesReturned;
    
    if (!DeviceIoControl(g_hDriver, IOCTL_APPLY_TEMP_PROFILE,
        &seed, sizeof(SEED_DATA),
        &tempProfile, sizeof(SYSTEM_PROFILE),
        &bytesReturned, NULL)) {
        printf("[Error] Failed to apply profile\n");
        CloseHandle(g_hDriver);
        return 1;
    }
    
    // Now get the active profile for hooking
    if (!DeviceIoControl(g_hDriver, IOCTL_GET_PROFILE,
        NULL, 0,
        &g_SpoofedProfile, sizeof(SYSTEM_PROFILE),
        &bytesReturned, NULL)) {
        printf("[Error] Failed to get profile\n");
        CloseHandle(g_hDriver);
        return 1;
    }
    
    if (bytesReturned != sizeof(SYSTEM_PROFILE)) {
        printf("[Error] Profile size mismatch: got %lu, expected %zu\n",
            bytesReturned, sizeof(SYSTEM_PROFILE));
        CloseHandle(g_hDriver);
        return 1;
    }
    
    printf("[Success] Profile loaded successfully\n");
    printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        g_SpoofedProfile.macAddress[0], g_SpoofedProfile.macAddress[1],
        g_SpoofedProfile.macAddress[2], g_SpoofedProfile.macAddress[3],
        g_SpoofedProfile.macAddress[4], g_SpoofedProfile.macAddress[5]);
    
    // Initialize hooks
    if (MH_Initialize() != MH_OK) {
        printf("[Error] MinHook initialization failed\n");
        return 1;
    }
    
    // Install GetSystemFirmwareTable hook
    InstallHooks();
    
    return 0;
}
```

---

## Expected Output (Fixed)

```
[7948] [HWIDSpoof] ========== DLL LOADING ==========
[7948] [HWIDSpoof] MinHook initialized
[7948] [HWIDSpoof] Applying temporary profile...
PCCleanup: IOCTL received: 0x222000
PCCleanup: IOCTL_APPLY_TEMP_PROFILE
PCCleanup: Temporary profile applied successfully
[7948] [HWIDSpoof] Profile applied, bytes returned: 555
[7948] [HWIDSpoof] Getting active profile...
PCCleanup: IOCTL received: 0x22201c
PCCleanup: IOCTL_GET_PROFILE requested
PCCleanup: Returned temporary profile to DLL
PCCleanup: MAC: 02:7A:FB:C3:C7:A7
[7948] [Success] Profile loaded: 555 bytes
[7948] [HWIDSpoof] GetSystemFirmwareTable hook installed
```

---

## Summary

? **Driver security fixed** - Non-admin access enabled  
? **IOCTL_GET_PROFILE implemented** - Returns active profile  
? **Cleanup on unload** - Restores original values  

? **DLL not applying profile first** - **YOU MUST FIX THIS!**  
? **MinHook stub** - Add real MinHook source files  

## Action Required

1. **Update your DLL initialization** to call `IOCTL_APPLY_TEMP_PROFILE` BEFORE `IOCTL_GET_PROFILE`
2. **Add MinHook source files** to your DLL project (see MINHOOK_FIX_GUIDE.md)
3. **Verify structure sizes match** between driver and DLL
4. **Rebuild and test**

The driver is working correctly - the issue is in your DLL initialization sequence!
