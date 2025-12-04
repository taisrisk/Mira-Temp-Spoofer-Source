#include "ProfileManager.h"
#include "RegistryHelper.h"
#include "NetworkSpoofing.h"
#include "SerialGenerator.h"
#include "GuidGenerator.h"
#include "SeedManager.h"
#include "PlatformDetector.h"
#include "NetworkCleaner.h"
#include "FormatDetector.h"
#include <ntstrsafe.h>

extern PROFILE_STATE g_ProfileState;

//
// BackupOriginalValues - Save original registry values
//
NTSTATUS BackupOriginalValues(VOID)
{
    NTSTATUS status;
    NTSTATUS finalStatus = STATUS_SUCCESS;
    WCHAR guidBuffer[64];
    ULONG resultLength = 0;

    PAGED_CODE();  // Assert we're at PASSIVE_LEVEL

    status = QueryRegistryValue(REG_CRYPTOGRAPHY, L"MachineGuid", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.machineGuid,
            sizeof(g_ProfileState.original.machineGuid),
            "%ws", guidBuffer);
    }
    else {
        finalStatus = status;
    }

    status = QueryRegistryValue(REG_HWPROFILE, L"HwProfileGuid", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.hwProfileGuid,
            sizeof(g_ProfileState.original.hwProfileGuid),
            "%ws", guidBuffer);
    }
    else {
        if (NT_SUCCESS(finalStatus)) {
            finalStatus = status;
        }
    }

    status = QueryRegistryValue(REG_CURRENTVERSION, L"ProductId", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.productId,
            sizeof(g_ProfileState.original.productId),
            "%ws", guidBuffer);
    }
    else {
        if (NT_SUCCESS(finalStatus)) {
            finalStatus = status;
        }
    }

    status = QueryRegistryValue(REG_BIOS, L"BIOSSerialNumber", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.biosSerial,
            sizeof(g_ProfileState.original.biosSerial),
            "%ws", guidBuffer);
    }

    status = QueryRegistryValue(REG_BIOS, L"BaseBoardSerialNumber", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.motherboardSerial,
            sizeof(g_ProfileState.original.motherboardSerial),
            "%ws", guidBuffer);
    }

    status = QueryRegistryValue(REG_BIOS, L"SystemSerialNumber", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.chassisSerial,
            sizeof(g_ProfileState.original.chassisSerial),
            "%ws", guidBuffer);
    }

    status = QueryRegistryValue(REG_BIOS, L"SystemUUID", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status)) {
        RtlStringCbPrintfA(g_ProfileState.original.systemUUID,
            sizeof(g_ProfileState.original.systemUUID),
            "%ws", guidBuffer);
    }

    return finalStatus;
}

//
// ApplyRegistryModifications - Apply temporary registry changes
//
NTSTATUS ApplyRegistryModifications(_In_ PSYSTEM_PROFILE profile)
{
    NTSTATUS status;
    WCHAR wideBuffer[128];
    ANSI_STRING ansiString;
    UNICODE_STRING unicodeString;

    // Add NULL check
    if (!profile) {
        KdPrint(("PCCleanup: NULL profile pointer!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    PAGED_CODE();  // Assert we're at PASSIVE_LEVEL

    // MachineGuid
    RtlInitAnsiString(&ansiString, profile->machineGuid);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_CRYPTOGRAPHY, L"MachineGuid", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // HwProfileGuid
    RtlInitAnsiString(&ansiString, profile->hwProfileGuid);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_HWPROFILE, L"HwProfileGuid", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // ProductId
    RtlInitAnsiString(&ansiString, profile->productId);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_CURRENTVERSION, L"ProductId", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // BIOS Serial
    RtlInitAnsiString(&ansiString, profile->biosSerial);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_BIOS, L"BIOSSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_BIOS, L"BIOSVendor", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_SYSTEMBIOS, L"BIOSSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // Motherboard Serial
    RtlInitAnsiString(&ansiString, profile->motherboardSerial);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_BIOS, L"BaseBoardSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_BIOS, L"BaseBoardProduct", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_SYSTEMBIOS, L"BaseBoardSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_SYSTEMBIOS, L"SystemProductName", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // Chassis Serial
    RtlInitAnsiString(&ansiString, profile->chassisSerial);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_BIOS, L"SystemSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_BIOS, L"SystemProductName", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_SYSTEMBIOS, L"SystemSerialNumber", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // System UUID
    RtlInitAnsiString(&ansiString, profile->systemUUID);
    unicodeString.Buffer = wideBuffer;
    unicodeString.MaximumLength = sizeof(wideBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
    if (NT_SUCCESS(status)) {
        SetRegistryValue(REG_BIOS, L"SystemUUID", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        SetRegistryValue(REG_SYSTEMBIOS, L"SystemUUID", REG_SZ,
            unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
    }

    // Network adapters MAC addresses
    KdPrint(("ProfileManager: ======== SPOOFING NETWORK ADAPTERS ========\n"));
    KdPrint(("ProfileManager: Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        profile->macAddress[0], profile->macAddress[1], profile->macAddress[2],
        profile->macAddress[3], profile->macAddress[4], profile->macAddress[5]));
    
    status = EnumerateAndSpoofNetworkAdapters(profile->macAddress);
    if (NT_SUCCESS(status)) {
        KdPrint(("ProfileManager: Network adapters spoofed successfully\n"));
        KdPrint(("ProfileManager: *** IMPORTANT: Adapters require restart for MAC to take effect ***\n"));
        
        // Auto-run network cleaner after MAC spoofing (initial cleanup)
        KdPrint(("ProfileManager: Running initial network cleanup...\n"));
        FlushDnsCache();
        ClearArpCache();
        KdPrint(("ProfileManager: Initial network cleanup complete\n"));
    }
    else {
        KdPrint(("ProfileManager: WARNING - Network adapter spoofing failed: 0x%08X\n", status));
    }

    // SMBIOS firmware tables
    KdPrint(("ProfileManager: Applying SMBIOS changes...\n"));
    status = ApplySMBIOSChanges(profile);
    if (NT_SUCCESS(status)) {
        KdPrint(("ProfileManager: SMBIOS changes applied successfully\n"));
    }
    else {
        KdPrint(("ProfileManager: WARNING - SMBIOS changes failed: 0x%08X\n", status));
    }

    KdPrint(("ProfileManager: ======== REGISTRY MODIFICATIONS COMPLETE ========\n"));

    return STATUS_SUCCESS;
}

//
// RestoreRegistryValues - Restore original values from backup
//
NTSTATUS RestoreRegistryValues(VOID)
{
    NTSTATUS status;
    WCHAR wideBuffer[128];
    ANSI_STRING ansiString;
    UNICODE_STRING unicodeString;

    PAGED_CODE();  // Assert we're at PASSIVE_LEVEL

    if (g_ProfileState.original.machineGuid[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.machineGuid);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_CRYPTOGRAPHY, L"MachineGuid", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.hwProfileGuid[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.hwProfileGuid);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_HWPROFILE, L"HwProfileGuid", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.productId[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.productId);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_CURRENTVERSION, L"ProductId", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.biosSerial[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.biosSerial);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_BIOS, L"BIOSSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_BIOS, L"BIOSVendor", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_SYSTEMBIOS, L"BIOSSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.motherboardSerial[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.motherboardSerial);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_BIOS, L"BaseBoardSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_BIOS, L"BaseBoardProduct", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_SYSTEMBIOS, L"BaseBoardSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_SYSTEMBIOS, L"SystemProductName", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.chassisSerial[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.chassisSerial);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_BIOS, L"SystemSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_BIOS, L"SystemProductName", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_SYSTEMBIOS, L"SystemSerialNumber", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    if (g_ProfileState.original.systemUUID[0] != '\0') {
        RtlInitAnsiString(&ansiString, g_ProfileState.original.systemUUID);
        unicodeString.Buffer = wideBuffer;
        unicodeString.MaximumLength = sizeof(wideBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE);
        if (NT_SUCCESS(status)) {
            SetRegistryValue(REG_BIOS, L"SystemUUID", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
            SetRegistryValue(REG_SYSTEMBIOS, L"SystemUUID", REG_SZ,
                unicodeString.Buffer, unicodeString.Length + sizeof(WCHAR));
        }
    }

    // Restore network adapters to hardware MAC
    KdPrint(("ProfileManager: Restoring network adapters to hardware MAC...\n"));
    status = RestoreAllNetworkAdapters();
    if (NT_SUCCESS(status)) {
        KdPrint(("ProfileManager: Network adapters restored successfully\n"));
        KdPrint(("ProfileManager: *** IMPORTANT: Restart adapters to apply hardware MAC ***\n"));
    }
    else {
        KdPrint(("ProfileManager: WARNING - Failed to restore network adapters: 0x%08X\n", status));
    }

    // Restore SMBIOS firmware tables
    RestoreSMBIOSTables();

    return STATUS_SUCCESS;
}

//
// GenerateSystemProfile - Create hardware profile using legacy seed
//
NTSTATUS GenerateSystemProfile(_In_ PSEED_DATA seed, _Out_ PSYSTEM_PROFILE profile)
{
    NTSTATUS status;
    PLATFORM_INFO platformInfo;
    UINT64 baseSeed = seed->seedValue ^ seed->timestamp;

    if (!seed || !profile) {
        KdPrint(("PCCleanup: NULL parameter in GenerateSystemProfile!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    PAGED_CODE();  // Assert we're at PASSIVE_LEVEL

    RtlZeroMemory(profile, sizeof(SYSTEM_PROFILE));

    status = DetectHardwarePlatform(&platformInfo);
    if (!NT_SUCCESS(status)) {
        platformInfo.Vendor = VENDOR_GENERIC;
    }

    status = GenerateMotherboardSerial(baseSeed, platformInfo.Vendor,
        profile->motherboardSerial, sizeof(profile->motherboardSerial));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = GenerateDiskSerial(baseSeed * 0x9E3779B97F4A7C15ULL,
        profile->diskSerial, sizeof(profile->diskSerial));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = GenerateBIOSSerial(baseSeed * 0x517CC1B727220A95ULL, platformInfo.Vendor,
        profile->biosSerial, sizeof(profile->biosSerial));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = GenerateChassisSerial(baseSeed * 0x85EBCA6B3B4F4A5FULL,
        profile->chassisSerial, sizeof(profile->chassisSerial));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = GenerateProductId(baseSeed,
        profile->productId, sizeof(profile->productId));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    GenerateGUID(baseSeed, profile->machineGuid, sizeof(profile->machineGuid));
    GenerateGUID(baseSeed ^ 0x123456789ABCDEFULL, profile->hwProfileGuid, sizeof(profile->hwProfileGuid));
    GenerateGUID(baseSeed ^ 0xFEDCBA9876543210ULL, profile->volumeGuid, sizeof(profile->volumeGuid));

    status = GenerateMACAddress(baseSeed ^ 0xBADC0FFEE0DDF00DULL, profile->macAddress);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    GenerateUUID(baseSeed, profile->systemUUID, sizeof(profile->systemUUID));

    return STATUS_SUCCESS;
}

//
// GenerateSystemProfileEnhanced - Create hardware profile using enhanced seed with format detection
//
NTSTATUS GenerateSystemProfileEnhanced(_In_ PENHANCED_SEED_DATA enhancedSeed, _Out_ PSYSTEM_PROFILE profile)
{
    NTSTATUS status;
    PLATFORM_INFO platformInfo;
    UINT64 baseSeed, primarySeed, secondarySeed, tertiarySeed;
    
    if (!enhancedSeed || !profile) {
        KdPrint(("PCCleanup: NULL parameter in GenerateSystemProfileEnhanced!\n"));
        return STATUS_INVALID_PARAMETER;
    }

    PAGED_CODE();  // Assert we're at PASSIVE_LEVEL
    
    // Get current values for format detection
    CHAR currentMotherboardSerial[64] = {0};
    CHAR currentBiosSerial[64] = {0};
    CHAR currentDiskSerial[64] = {0};
    
    // Read current values from registry for format detection
    WCHAR guidBuffer[128];
    ULONG resultLength = 0;
    
    KdPrint(("PCCleanup: Reading current hardware serials for format detection...\n"));
    
    status = QueryRegistryValue(REG_BIOS, L"BaseBoardSerialNumber", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status) && resultLength > 0) {
        RtlStringCbPrintfA(currentMotherboardSerial, sizeof(currentMotherboardSerial), "%ws", guidBuffer);
    }
    
    status = QueryRegistryValue(REG_BIOS, L"BIOSSerialNumber", guidBuffer, sizeof(guidBuffer), &resultLength);
    if (NT_SUCCESS(status) && resultLength > 0) {
        RtlStringCbPrintfA(currentBiosSerial, sizeof(currentBiosSerial), "%ws", guidBuffer);
    }
    
    // Disk serial detection (simplified - using generic for now)
    // In production, you'd query via STORAGE_DEVICE_DESCRIPTOR
    
    // Detect formats
    SERIAL_FORMAT motherboardFormat = DetectMotherboardFormat(currentMotherboardSerial);
    SERIAL_FORMAT biosFormat = DetectBIOSFormat(currentBiosSerial);
    DISK_FORMAT diskFormat = DISK_FORMAT_SAMSUNG; // Default to Samsung format
    
    KdPrint(("PCCleanup: Detected formats - Motherboard: %d, BIOS: %d, Disk: %d\n",
        motherboardFormat, biosFormat, diskFormat));
    KdPrint(("PCCleanup: Current Motherboard: %s\n", currentMotherboardSerial[0] ? currentMotherboardSerial : "(empty)"));
    KdPrint(("PCCleanup: Current BIOS: %s\n", currentBiosSerial[0] ? currentBiosSerial : "(empty)"));
    
    // Generate seeds
    baseSeed = enhancedSeed->baseSeed ^ enhancedSeed->entropyMix;
    primarySeed = HashValue(baseSeed);
    secondarySeed = HashValue(baseSeed ^ enhancedSeed->timestamp);
    tertiarySeed = HashValue(baseSeed ^ (UINT64)enhancedSeed->platformFlags);
    
    // Detect platform vendor
    if (enhancedSeed->platformFlags != 0) {
        status = DecodePlatformFlags(enhancedSeed->platformFlags, &platformInfo);
        if (!NT_SUCCESS(status)) {
            status = DetectHardwarePlatform(&platformInfo);
        }
    } else {
        status = DetectHardwarePlatform(&platformInfo);
    }
    
    if (!NT_SUCCESS(status)) {
        platformInfo.Vendor = VENDOR_GENERIC;
        platformInfo.IsIntel = TRUE;
    }

    // Apply manufacturer hint if provided
    if (enhancedSeed->manufacturerHint != 0) {
        MOTHERBOARD_VENDOR hintVendor = (MOTHERBOARD_VENDOR)(enhancedSeed->manufacturerHint % 6);
        if (hintVendor <= VENDOR_GENERIC) {
            platformInfo.Vendor = hintVendor;
        }
    }
    
    // Generate motherboard serial in detected format
    status = GenerateInFormat_Motherboard(
        primarySeed,
        motherboardFormat != FORMAT_UNKNOWN ? motherboardFormat : FORMAT_ASUS,
        currentMotherboardSerial[0] ? currentMotherboardSerial : NULL,
        profile->motherboardSerial,
        sizeof(profile->motherboardSerial)
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: Failed to generate motherboard serial\n"));
        return status;
    }
    
    // Generate BIOS serial in detected format
    status = GenerateInFormat_BIOS(
        tertiarySeed,
        biosFormat != FORMAT_UNKNOWN ? biosFormat : FORMAT_GENERIC,
        currentBiosSerial[0] ? currentBiosSerial : NULL,
        profile->biosSerial,
        sizeof(profile->biosSerial)
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: Failed to generate BIOS serial\n"));
        return status;
    }
    
    // Generate disk serial in detected format
    status = GenerateInFormat_Disk(
        secondarySeed,
        diskFormat,
        currentDiskSerial[0] ? currentDiskSerial : NULL,
        profile->diskSerial,
        sizeof(profile->diskSerial)
    );
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: Failed to generate disk serial\n"));
        return status;
    }
    
    // Generate system serial (Type 1, Field 4) - varies by manufacturer
    CHAR systemSerial[64];
    status = GenerateSystemSerialByVendor(secondarySeed, platformInfo.Vendor,
        NULL, systemSerial, sizeof(systemSerial));
    if (!NT_SUCCESS(status)) {
        RtlStringCbCopyA(systemSerial, sizeof(systemSerial), "Not Specified");
    }
    
    // Store system serial in chassisSerial field (Type 1, Field 4 maps to this)
    RtlStringCbCopyA(profile->chassisSerial, sizeof(profile->chassisSerial), systemSerial);
    
    // Generate product serial - THIS GOES INTO Type 1, Field 6
    status = GenerateProductSerialByVendor(tertiarySeed, platformInfo.Vendor,
        NULL, profile->productId, sizeof(profile->productId));
    if (!NT_SUCCESS(status)) {
        // Fallback to Windows Product ID format
        status = GenerateProductId(baseSeed, profile->productId, sizeof(profile->productId));
    }
    
    // Generate GUIDs (standardized formats)
    UINT64 guidSeed1 = HashValue(primarySeed ^ 0x123456789ABCDEFULL);
    UINT64 guidSeed2 = HashValue(secondarySeed ^ 0xFEDCBA9876543210ULL);
    UINT64 guidSeed3 = HashValue(tertiarySeed ^ 0xC0FFEEBABEDEAFULL);

    GenerateGUID(guidSeed1, profile->machineGuid, sizeof(profile->machineGuid));
    GenerateGUID(guidSeed2, profile->hwProfileGuid, sizeof(profile->hwProfileGuid));
    GenerateGUID(guidSeed3, profile->volumeGuid, sizeof(profile->volumeGuid));
    
    // Generate MAC address
    UINT64 macSeed = HashValue(baseSeed ^ 0xBADC0FFEE0DDF00DULL);
    status = GenerateMACAddress(macSeed, profile->macAddress);
    if (!NT_SUCCESS(status)) {
        KdPrint(("PCCleanup: Failed to generate MAC address\n"));
        return status;
    }
    
    // Generate proper UUID v4 format for System UUID
    UINT64 uuidSeed = HashValue(primarySeed ^ secondarySeed ^ tertiarySeed);
    UCHAR uuidBytes[16];
    
    // Generate 16 random bytes from seed
    for (int i = 0; i < 16; i++) {
        uuidSeed = HashValue(uuidSeed + i);
        uuidBytes[i] = (UCHAR)(uuidSeed & 0xFF);
    }

    // Set UUID version 4 and variant bits
    uuidBytes[6] = (uuidBytes[6] & 0x0F) | 0x40;  // Version 4
    uuidBytes[8] = (uuidBytes[8] & 0x3F) | 0x80;  // Variant 2

    // Format as UUID string: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    RtlStringCbPrintfA(profile->systemUUID, sizeof(profile->systemUUID),
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        uuidBytes[0], uuidBytes[1], uuidBytes[2], uuidBytes[3],
        uuidBytes[4], uuidBytes[5],
        uuidBytes[6], uuidBytes[7],
        uuidBytes[8], uuidBytes[9],
        uuidBytes[10], uuidBytes[11], uuidBytes[12], uuidBytes[13], uuidBytes[14], uuidBytes[15]);
    
    KdPrint(("PCCleanup: ======== PROFILE GENERATED WITH FORMAT PRESERVATION ========\n"));
    KdPrint(("PCCleanup: Motherboard: %s (format: %d)\n", profile->motherboardSerial, motherboardFormat));
    KdPrint(("PCCleanup: BIOS: %s (format: %d)\n", profile->biosSerial, biosFormat));
    KdPrint(("PCCleanup: Disk: %s (format: %d)\n", profile->diskSerial, diskFormat));
    KdPrint(("PCCleanup: System Serial: %s\n", profile->chassisSerial));
    KdPrint(("PCCleanup: Product Serial: %s\n", profile->productId));
    KdPrint(("PCCleanup: System UUID: %s\n", profile->systemUUID));
    KdPrint(("PCCleanup: MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        profile->macAddress[0], profile->macAddress[1], profile->macAddress[2],
        profile->macAddress[3], profile->macAddress[4], profile->macAddress[5]));
    KdPrint(("PCCleanup: ================================================================\n"));

    return STATUS_SUCCESS;
}
