<div align="center">

# Mira Temp Spoofer by zrorisc

### Advanced Hardware Identity Spoofing System

[![Release](https://img.shields.io/badge/release-v1.0-blue.svg)](https://github.com/taisrisk/Mira-Temp-Spoofer-Source/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-brightgreen.svg)]()
[![License](https://img.shields.io/badge/license-MIT-orange.svg)]()
[![Build](https://img.shields.io/badge/build-passing-success.svg)]()

*Professional-grade hardware identifier spoofing for Windows kernel environments*

[Features](#-features) • [Architecture](#-architecture) • [Installation](#-installation) • [Usage](#-usage) • [Security](#-security)

</div>

---

## ?? Overview

**Mira Temp Spoofer** is a sophisticated Windows kernel driver that provides temporary hardware identity spoofing capabilities. Built using pure WDM (Windows Driver Model), it offers comprehensive modification of system identifiers including SMBIOS tables, registry values, MAC addresses, and hardware profiles.

### Key Capabilities

- **Temporary Hardware Spoofing** - All changes are reversible and automatically restored on driver unload
- **Format-Aware Generation** - Detects and preserves manufacturer-specific serial formats (ASUS, MSI, Gigabyte, Dell, HP, etc.)
- **Cryptographically Secure** - Hardware-based entropy collection and cryptographic random number generation
- **Anti-Cheat Compatible** - Designed to evade detection by modern anti-cheat systems
- **Platform Detection** - Automatically identifies motherboard vendors and chipset configurations
- **Multi-Layer Cleaning** - Removes traces from Epic Games, Xbox, and network history

---

## ? Features

### ?? Hardware Spoofing

- **SMBIOS Table Modification**
  - BIOS serial numbers and version strings
  - Motherboard serial numbers (vendor-specific formats)
  - Chassis serial numbers and asset tags
  - System UUID and product identifiers

- **Registry Spoofing**
  - Machine GUID (`HKLM\SOFTWARE\Microsoft\Cryptography`)
  - Hardware Profile GUID
  - Product ID and installation identifiers
  - Volume GUID and disk signatures

- **Network Adapter Spoofing**
  - MAC address modification (locally administered addresses)
  - Support for Ethernet, Wi-Fi, TAP, and VPN adapters
  - Dual registry key spoofing (`NetworkAddress` + `PermanentAddress`)

### ?? Trace Removal

- **Epic Games Cleaner**
  - 13+ registry path cleanup
  - Application cache and log removal
  - User profile trace deletion

- **Xbox Identity Cleaner**
  - Xbox Game Bar data removal
  - Identity provider cleanup
  - Xbox app trace deletion

- **Network History Cleaner**
  - DNS cache flushing
  - ARP table clearing
  - NetBIOS and DHCP lease cleanup
  - Network connection history removal

### ?? Profile Generation

- **Seed-Based Generation**
  - User-provided seeds for reproducibility
  - True random generation using hardware entropy
  - Platform-aware seed generation
  - Timestamp-based variants

- **Format Detection & Preservation**
  - Auto-detects 9+ motherboard serial formats
  - Preserves vendor-specific patterns
  - Supports ASUS, MSI, Gigabyte, ASRock, Intel, Dell, HP, Lenovo
  - Disk serial format preservation (Samsung, WD, Seagate, NVMe)

### ?? Security Features

- **Cryptographic RNG**
  - Hardware-based entropy collection
  - Performance counter mixing
  - CPU cycle randomization
  - Stack and pool address entropy

- **Safe Operations**
  - Passive-level IRQL operations
  - Buffered I/O for kernel-user communication
  - Automatic restoration on driver unload
  - Secure memory wiping (`RtlSecureZeroMemory`)

---

## ??? Architecture

### Driver Components

```
Mira Temp Spoofer
+-- Core Driver (WDM)
¦   +-- Driver.c/h          - Entry point, IOCTL dispatcher
¦   +-- ProfileManager      - Profile generation and state management
¦
+-- Cryptography
¦   +-- CryptoProvider      - Kernel-native RNG initialization
¦   +-- EntropyCollector    - Hardware entropy harvesting
¦   +-- GuidGenerator       - UUID/GUID generation
¦
+-- Hardware Detection
¦   +-- PlatformDetector    - Motherboard vendor identification
¦   +-- FormatDetector      - Serial format pattern recognition
¦
+-- Serial Generation
¦   +-- SerialGenerator     - Vendor-specific serial creation
¦   +-- SeedManager         - Seed generation and persistence
¦
+-- Spoofing Modules
¦   +-- NetworkSpoofing     - MAC address modification
¦   +-- RegistryHelper      - Registry value manipulation
¦
+-- Trace Removal
    +-- EpicCleaner         - Epic Games trace removal
    +-- XboxCleaner         - Xbox identity cleanup
    +-- NetworkCleaner      - Network history clearing
```

### IOCTL Interface

| IOCTL Code | Function | Description |
|------------|----------|-------------|
| `0x800` | `APPLY_TEMP_PROFILE` | Generate and apply hardware profile |
| `0x801` | `RESTORE_PROFILE` | Restore original hardware identifiers |
| `0x802` | `GET_STATUS` | Check driver operational status |
| `0x803` | `CLEAN_EPIC_TRACES` | Remove Epic Games traces |
| `0x804` | `GENERATE_SEED` | Generate cryptographic seed |
| `0x805` | `CLEAN_NETWORK_TRACES` | Clear network history |
| `0x806` | `CLEAN_XBOX_TRACES` | Remove Xbox identity traces |
| `0x807` | `GET_PROFILE` | Retrieve active profile data |
| `0x808` | `RESTART_ADAPTERS` | Get adapter restart instructions |
| `0x809` | `INTEGRATED_CLEANUP` | Execute full system cleanup |

### Data Structures

```cpp
// Enhanced seed with platform awareness
typedef struct _ENHANCED_SEED_DATA {
    UINT64 baseSeed;
    UINT64 timestamp;
    UINT64 entropyMix;
    UINT32 platformFlags;
    UINT16 manufacturerHint;
    CHAR seedId[16];
} ENHANCED_SEED_DATA;

// System profile (555 bytes)
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
} SYSTEM_PROFILE;
```

---

## ?? Installation

### Prerequisites

- Windows 10/11 (x64)
- Administrator privileges
- Test signing enabled OR driver signature disabled

### Method 1: KDU Loader (Recommended)

```powershell
# Load driver using KDU
kdu.exe -map MiraCleaner.sys
```

### Method 2: DSEFix

```powershell
# Disable Driver Signature Enforcement
bcdedit /set nointegritychecks on
bcdedit /set testsigning on

# Reboot, then load driver
sc create MiraDriver type= kernel binPath= C:\path\to\MiraCleaner.sys
sc start MiraDriver
```

### Method 3: Test Signing

```powershell
# Enable test signing mode
bcdedit /set testsigning on

# Reboot and load signed driver
sc create MiraDriver type= kernel binPath= C:\path\to\MiraCleaner.sys
sc start MiraDriver
```

---

## ?? Usage

### Basic Spoofing

```cpp
// 1. Open driver handle
HANDLE hDriver = CreateFile("\\\\.\\PCCleanupDriver", 
    GENERIC_READ | GENERIC_WRITE, 0, NULL, 
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

// 2. Prepare seed data
SEED_DATA seed;
seed.seedValue = 0x1234567890ABCDEF;
seed.timestamp = GetTickCount64();

// 3. Apply temporary profile
SYSTEM_PROFILE profile;
DWORD bytesReturned;
DeviceIoControl(hDriver, IOCTL_APPLY_TEMP_PROFILE, 
    &seed, sizeof(SEED_DATA), 
    &profile, sizeof(SYSTEM_PROFILE), 
    &bytesReturned, NULL);

// 4. Restart network adapters for MAC changes
system("netsh interface set interface name=\"Wi-Fi\" admin=DISABLED");
Sleep(2000);
system("netsh interface set interface name=\"Wi-Fi\" admin=ENABLED");

// 5. Restore original values
DeviceIoControl(hDriver, IOCTL_RESTORE_PROFILE, 
    NULL, 0, NULL, 0, &bytesReturned, NULL);

CloseHandle(hDriver);
```

### Trace Removal

```cpp
// Clean Epic Games traces
EPIC_CLEANUP_STATS epicStats;
DeviceIoControl(hDriver, IOCTL_CLEAN_EPIC_TRACES, 
    NULL, 0, &epicStats, sizeof(epicStats), &bytesReturned, NULL);

// Clean network traces
DeviceIoControl(hDriver, IOCTL_CLEAN_NETWORK_TRACES, 
    NULL, 0, NULL, 0, &bytesReturned, NULL);

// Clean Xbox traces
DeviceIoControl(hDriver, IOCTL_CLEAN_XBOX_TRACES, 
    NULL, 0, NULL, 0, &bytesReturned, NULL);
```

### DLL Integration

For runtime hooking and SMBIOS table interception:

```cpp
BOOL InitializeHWIDSpoof()
{
    // 1. Open driver
    g_hDriver = CreateFile("\\\\.\\PCCleanupDriver", ...);
    
    // 2. Apply profile FIRST
    DeviceIoControl(g_hDriver, IOCTL_APPLY_TEMP_PROFILE, ...);
    
    // 3. Get active profile for hooks
    DeviceIoControl(g_hDriver, IOCTL_GET_PROFILE, ...);
    
    // 4. Initialize MinHook
    MH_Initialize();
    
    // 5. Hook GetSystemFirmwareTable
    MH_CreateHookApiEx(L"kernel32", "GetSystemFirmwareTable", 
        &Hook_GetSystemFirmwareTable, ...);
    MH_EnableHook(MH_ALL_HOOKS);
}
```

---

## ?? Security

### Cryptographic Strength

- **Hardware Entropy Sources:**
  - `KeQueryPerformanceCounter` - High-resolution timing
  - `KeQuerySystemTime` - System time variations
  - `KeQueryTickCount` - Tick count deltas
  - CPU cycle counters - Processor-level randomness
  - Stack/pool addresses - Memory layout entropy

- **Entropy Mixing:**
  - Advanced hash functions with multiple rounds
  - XOR mixing of multiple entropy sources
  - Timestamp-based seed variants

### Anti-Detection Measures

- **Format Preservation:** Maintains vendor-specific serial formats to avoid pattern detection
- **Locally Administered MACs:** Uses proper MAC address formatting (02:xx:xx:xx:xx:xx)
- **Realistic UUIDs:** Generates valid UUID v4 format identifiers
- **SMBIOS Compliance:** Preserves table structure and checksums
- **Registry Consistency:** Maintains proper GUID and serial formats

### Safe Operation

- **Automatic Restoration:** Driver unload triggers full restoration of original values
- **Buffered I/O:** Safe kernel-user communication without direct pointer access
- **PASSIVE_LEVEL:** All operations execute at safe IRQL level
- **Error Handling:** Comprehensive error checking with automatic rollback
- **Memory Safety:** Secure memory wiping after sensitive operations

---

## ??? Building from Source

### Requirements

- Visual Studio 2019/2022
- Windows Driver Kit (WDK) 10.0.26100.0 or newer
- Windows SDK matching WDK version

### Build Steps

1. Clone the repository:
```bash
git clone https://github.com/taisrisk/Mira-Temp-Spoofer-Source
cd Mira-Temp-Spoofer-Source
```

2. Open `Mira Cleaner.sln` in Visual Studio

3. Select build configuration:
   - Configuration: `Release`
   - Platform: `x64`

4. Build solution (`Ctrl+Shift+B`)

5. Output: `x64\Release\MiraCleaner.sys`

### Configuration Notes

- **Driver Type:** WDM (not WDF)
- **C++ Standard:** C++14
- **Target Platform:** Windows 10/11 x64
- **Dependencies:** `ntoskrnl.lib`, `hal.lib`

---

## ?? Supported Platforms

### Motherboard Vendors
- ? ASUS
- ? MSI
- ? Gigabyte
- ? ASRock
- ? Intel
- ? Dell
- ? HP
- ? Lenovo

### Disk Manufacturers
- ? Samsung (S4NXXXXXXXXX format)
- ? Western Digital (WD-WCCXXXXXXXXXXXX format)
- ? Seagate (XXXXXXXX format)
- ? NVMe (NVME-XXXXXXXXXXXX format)

### Network Adapters
- ? Ethernet (all vendors)
- ? Wi-Fi (all vendors)
- ? TAP-Windows
- ? VPN adapters

---

## ?? Debugging

### Enable Debug Output

Use **DebugView** to monitor kernel logs:

1. Download [DebugView](https://docs.microsoft.com/sysinternals/downloads/debugview)
2. Enable: `Capture Kernel` + `Capture Win32`
3. Filter: `PCCleanup`

### Verify Changes

```powershell
# Check MAC address
getmac

# Check System UUID
wmic csproduct get uuid

# Check Motherboard Serial
wmic baseboard get serialnumber

# Check BIOS Serial
wmic bios get serialnumber

# Check Registry
reg query "HKLM\SOFTWARE\Microsoft\Cryptography" /v MachineGuid
```

---

## ?? Important Notes

### MAC Address Changes

After spoofing MAC addresses, network adapters **must be restarted** for changes to take effect:

```powershell
netsh interface set interface name="Wi-Fi" admin=DISABLED
timeout /t 2
netsh interface set interface name="Wi-Fi" admin=ENABLED
```

Or reboot the system for changes to apply.

### Restoration

- Driver automatically restores original values on unload
- Manual restoration available via `IOCTL_RESTORE_PROFILE`
- Backup is created before first modification

### Compatibility

- Tested on Windows 10 (versions 1909-22H2) and Windows 11
- Requires administrator privileges
- May conflict with some anti-cheat systems
- Not compatible with virtualization detection systems

---

## ?? License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## ?? Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

---

## ?? Disclaimer

This software is provided for educational and research purposes only. Users are responsible for compliance with all applicable laws and terms of service. The authors assume no liability for misuse of this software.

---

## ?? Support

For issues, questions, or contributions:
- **GitHub Issues:** [Report a bug](https://github.com/taisrisk/Mira-Temp-Spoofer-Source/issues)
- **Pull Requests:** [Contribute code](https://github.com/taisrisk/Mira-Temp-Spoofer-Source/pulls)

---

<div align="center">

**Built with ?? for the Windows kernel community**

*Copyright © 2025 - All rights reserved*

</div>
