#include "FormatDetector.h"
#include <ntstrsafe.h>

//
// DetectMotherboardFormat - Detect current motherboard serial format
//
SERIAL_FORMAT DetectMotherboardFormat(_In_ PCSTR CurrentSerial) {
    if (!CurrentSerial || strlen(CurrentSerial) == 0) {
        return FORMAT_UNKNOWN;
    }
    
    size_t len = strlen(CurrentSerial);
    
    // ASUS: [Year][Month][10-digit sequence] - e.g., "5B1234567890" (12 chars)
    if (len == 12 && CurrentSerial[0] >= '0' && CurrentSerial[0] <= '9' &&
        ((CurrentSerial[1] >= 'A' && CurrentSerial[1] <= 'C') || 
         (CurrentSerial[1] >= '0' && CurrentSerial[1] <= '9'))) {
        return FORMAT_ASUS;
    }
    
    // MSI: "601-XXXX-XXXSBYYMM" - e.g., "601-7C02-012SB2105"
    if (len == 17 && CurrentSerial[0] == '6' && CurrentSerial[1] == '0' && 
        CurrentSerial[2] == '1' && CurrentSerial[3] == '-') {
        return FORMAT_MSI;
    }
    
    // Gigabyte: "SNYYMMXXXXXX" - e.g., "SN2511034567"
    if (len == 12 && CurrentSerial[0] == 'S' && CurrentSerial[1] == 'N') {
        return FORMAT_GIGABYTE;
    }
    
    // ASRock
    if (strstr(CurrentSerial, "To Be Filled") || strstr(CurrentSerial, "O.E.M.")) {
        return FORMAT_ASROCK;
    }
    
    // Intel: "JKFXXXXIRXXXXXX" - e.g., "JKF2224IR515000"
    if (len >= 10 && CurrentSerial[0] == 'J' && CurrentSerial[1] == 'K' && CurrentSerial[2] == 'F') {
        return FORMAT_INTEL;
    }
    
    return FORMAT_GENERIC;
}

//
// DetectBIOSFormat - Detect BIOS serial format (often different from motherboard)
//
SERIAL_FORMAT DetectBIOSFormat(_In_ PCSTR CurrentSerial) {
    if (!CurrentSerial || strlen(CurrentSerial) == 0) {
        return FORMAT_UNKNOWN;
    }
    
    size_t len = strlen(CurrentSerial);
    
    // Dell: 7 characters alphanumeric
    if (len == 7) {
        BOOLEAN allAlphaNum = TRUE;
        for (size_t i = 0; i < len; i++) {
            if (!((CurrentSerial[i] >= '0' && CurrentSerial[i] <= '9') ||
                  (CurrentSerial[i] >= 'A' && CurrentSerial[i] <= 'Z'))) {
                allAlphaNum = FALSE;
                break;
            }
        }
        if (allAlphaNum) {
            return FORMAT_DELL;
        }
    }
    
    // HP: "CNV" prefix + 7 digits
    if (len == 10 && CurrentSerial[0] == 'C' && CurrentSerial[1] == 'N' && CurrentSerial[2] == 'V') {
        return FORMAT_HP;
    }
    
    // Lenovo: "P" + 7-8 characters
    if (len >= 8 && len <= 9 && CurrentSerial[0] == 'P') {
        return FORMAT_LENOVO;
    }
    
    // Often "Default string" or "Not Specified"
    if (strstr(CurrentSerial, "Default") || strstr(CurrentSerial, "Not Specified")) {
        return FORMAT_GENERIC;
    }
    
    // Otherwise, use same detection as motherboard
    return DetectMotherboardFormat(CurrentSerial);
}

//
// DetectDiskFormat - Detect disk serial format
//
DISK_FORMAT DetectDiskFormat(_In_ PCSTR CurrentSerial) {
    if (!CurrentSerial || strlen(CurrentSerial) == 0) {
        return DISK_FORMAT_GENERIC;
    }
    
    // Samsung SSD: "S4NX" or "S4N" prefix
    if ((CurrentSerial[0] == 'S' && CurrentSerial[1] == '4' && CurrentSerial[2] == 'N')) {
        return DISK_FORMAT_SAMSUNG;
    }
    
    // Western Digital: "WD-WCC" prefix
    if ((CurrentSerial[0] == 'W' && CurrentSerial[1] == 'D' && CurrentSerial[2] == '-') ||
        (CurrentSerial[0] == 'W' && CurrentSerial[1] == 'C' && CurrentSerial[2] == 'C')) {
        return DISK_FORMAT_WD;
    }
    
    // NVMe: Custom "NVME-" prefix (common in modern SSDs)
    if (CurrentSerial[0] == 'N' && CurrentSerial[1] == 'V' && CurrentSerial[2] == 'M' && CurrentSerial[3] == 'E') {
        return DISK_FORMAT_NVME;
    }
    
    // Seagate: Pure numeric, usually 16 chars
    BOOLEAN allNumeric = TRUE;
    size_t len = strlen(CurrentSerial);
    for (size_t i = 0; i < len; i++) {
        if (!(CurrentSerial[i] >= '0' && CurrentSerial[i] <= '9')) {
            allNumeric = FALSE;
            break;
        }
    }
    if (allNumeric && len >= 12) {
        return DISK_FORMAT_SEAGATE;
    }
    
    return DISK_FORMAT_GENERIC;
}

//
// GenerateInFormat_Motherboard - Generate serial matching user's current format
//
NTSTATUS GenerateInFormat_Motherboard(
    _In_ UINT64 Seed,
    _In_ SERIAL_FORMAT Format,
    _In_opt_ PCSTR Template,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize
)
{
    UINT64 hash = Seed;
    
    // Extract date components from seed
    UINT32 year = (UINT32)((hash >> 8) % 10);  // 0-9
    UINT32 month = (UINT32)((hash >> 16) % 12) + 1; // 1-12
    UINT64 sequence = hash >> 24;
    
    CHAR monthChar;
    if (month >= 10) {
        monthChar = 'A' + (CHAR)(month - 10);
    } else {
        monthChar = '0' + (CHAR)month;
    }
    
    switch (Format) {
        case FORMAT_ASUS:
            // [Year][Month][10-digit sequence]
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "%d%c%010llu",
                year, monthChar, sequence % 10000000000ULL);
        
        case FORMAT_MSI:
            // 601-XXXX-XXXSBYYMM
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "601-%04X-%03XSB%02d%02d",
                (USHORT)(hash & 0xFFFF),
                (USHORT)((hash >> 16) & 0xFFF),
                20 + year, month);
        
        case FORMAT_GIGABYTE:
            // SNYYMMXXXXXX
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "SN%02d%02d%06llu",
                20 + year, month, sequence % 1000000ULL);
        
        case FORMAT_ASROCK:
            return RtlStringCbCopyA(Buffer, BufferSize, "To Be Filled By O.E.M.");
        
        case FORMAT_INTEL:
            // JKFXXXXIRXXXXXX
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "JKF%04X%08llu",
                (USHORT)(hash & 0xFFFF),
                sequence % 100000000ULL);
        
        default:
            // Generic: use template if provided, otherwise generate similar format
            if (Template && strlen(Template) > 0) {
                // Copy template structure, replace with new random values
                RtlStringCbCopyA(Buffer, BufferSize, Template);
                // Replace last 6 chars with random
                size_t len = strlen(Buffer);
                if (len >= 6) {
                    RtlStringCbPrintfA(Buffer + len - 6, 7, "%06llu", sequence % 1000000ULL);
                }
                return STATUS_SUCCESS;
            }
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "%d%c%010llu", year, monthChar, sequence % 10000000000ULL);
    }
}

//
// GenerateInFormat_BIOS - Generate BIOS serial in matching format
//
NTSTATUS GenerateInFormat_BIOS(
    _In_ UINT64 Seed,
    _In_ SERIAL_FORMAT Format,
    _In_opt_ PCSTR Template,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize
)
{
    UINT64 hash = Seed;
    
    switch (Format) {
        case FORMAT_DELL:
            // 7 chars alphanumeric
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "%07llu", hash % 10000000ULL);
        
        case FORMAT_HP:
            // CNV + 7 digits
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "CNV%07llu", hash % 10000000ULL);
        
        case FORMAT_LENOVO:
            // P + 7 chars
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "P%07llu", hash % 10000000ULL);
        
        case FORMAT_GENERIC:
        case FORMAT_UNKNOWN:
            return RtlStringCbCopyA(Buffer, BufferSize, "Default string");
        
        default:
            // Use motherboard format if no BIOS-specific format
            return GenerateInFormat_Motherboard(Seed, Format, Template, Buffer, BufferSize);
    }
}

//
// GenerateInFormat_Disk - Generate disk serial in matching format
//
NTSTATUS GenerateInFormat_Disk(
    _In_ UINT64 Seed,
    _In_ DISK_FORMAT Format,
    _In_opt_ PCSTR Template,
    _Out_ PCHAR Buffer,
    _In_ SIZE_T BufferSize
)
{
    UINT64 hash = Seed;
    
    switch (Format) {
        case DISK_FORMAT_SAMSUNG:
            // S4NX + 11 chars
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "S4NX%011llu", hash % 100000000000ULL);
        
        case DISK_FORMAT_WD:
            // WD-WCC + 12 chars
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "WD-WCC%012llu", hash % 1000000000000ULL);
        
        case DISK_FORMAT_NVME:
            // NVME- + 12 hex chars
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "NVME-%012llX", hash);
        
        case DISK_FORMAT_SEAGATE:
            // 16 numeric digits
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "%08llu%08llu",
                (hash % 100000000ULL),
                ((hash >> 32) % 100000000ULL));
        
        default:
            if (Template && strlen(Template) > 0) {
                // Try to preserve template structure
                RtlStringCbCopyA(Buffer, BufferSize, Template);
                size_t len = strlen(Buffer);
                if (len >= 8) {
                    RtlStringCbPrintfA(Buffer + len - 8, 9, "%08llu", hash % 100000000ULL);
                }
                return STATUS_SUCCESS;
            }
            // Generic: alphanumeric 12-16 chars
            return RtlStringCbPrintfA(Buffer, BufferSize,
                "%012llX", hash);
    }
}