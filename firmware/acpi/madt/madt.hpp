#pragma once

#include "../acpi.hpp"

// Register I/O APIC diakses secara tidak langsung
#define IOAPIC_REG_SEL 0x00 // Register Select
#define IOAPIC_REG_WIN 0x10 // Register Window (Data)

// ID dari entri REDTBL (0-23)
// GSI (Global System Interrupt) 1 adalah Keyboard
#define IOAPIC_REDTBL_START 0x10
#define REDTBL_ENTRY_FOR_GSI(gsi) (IOAPIC_REDTBL_START + (gsi * 2))

#define MAX_IOAPIC_COUNT 16

typedef U32 IOAPICFLAGS;

#define LAPIC_REG_SIVR 0x0F0 // Spurious Interrupt Vector Register
#define APIC_SOFTWARE_ENABLE (1U << 8)

#define MADT_MULTI_LOCAL_APIC 0
#define MADT_IO_APIC 1
#define MADT_ISO_APIC 2


// Default flags for I/O APIC redirection entries:
// - IOAPIC_FLAGS_DEFAULT: Active high, edge-triggered, not masked (0x00000000)
// - IOAPIC_FLAG_MASKED: Mask the redirection entry (sets the mask bit)
#define IOAPIC_FLAGS_DEFAULT  ((IOAPICFLAGS)0x00000000)
#define IOAPIC_FLAG_MASKED    ((IOAPICFLAGS)0x00010000)
#define IOAPIC_FLAGS_LEVEL (1 << 15)
#define IOAPIC_FLAGS_LOW (1 << 13)

// MODE MODE APIC
#define ROS_APIC_MODE_PIC          0x00  // Mode 8259 lama (Legacy)
#define ROS_APIC_MODE_VIRTUAL_WIRE 0x01  // Mode transisi (APIC lewat PIC)
#define ROS_APIC_MODE_SYMMETRIC_IO 0x10  // Ini target kita (Full APIC)


#define CONFIG_TIMER_HEXA_GLOBAL 0xEE

namespace ACPI { namespace LAPIC { extern volatile U8* g_LapicVirtualBase; } }

namespace ACPI {
    // Public declaration for MADT parser (implemented in madt.cpp)
    VOID ParseMADT();

    // Expose LAPIC and IOAPIC init helpers for callers (e.g. ACPI initializer,
    // irq handler). Implementations are in madt.cpp.
    namespace LAPIC {
        VOID InitializeLAPIC();
        VOID LapicWrite(U32 RegOffset, U32 Value);
        U32  LapicRead(U32 RegOffset);
    }

    namespace IOAPIC {
        VOID InitializeIOAPIC();
        VOID IOApicRedirect(U8 GSI, U8 Vector, IOAPICFLAGS Flags);
        // Redirect GSI to a specific destination APIC ID (dest in high dword bits 31:24)
        VOID IOApicRedirectToCPU(U8 GSI, U8 Vector, IOAPICFLAGS Flags, U8 destApicId);
        // Read/Write an IOAPIC register (data window access)
        VOID IOAPICWrite(U8 RegOffset, U32 Value);
        U32 IOAPICRead(U8 RegOffset);
    }

	typedef struct _ROS_ACPI_INFO_TABLE{
		U32 APICMode;
		U32 ProccessorCount;
		U32 IoAPICCount;
		U64 LocalAPICPA;
		U64 IoAPICVA[MAX_IOAPIC_COUNT];
		U64 IoAPICPA[MAX_IOAPIC_COUNT];
		U32 IoAPICIRQBase[MAX_IOAPIC_COUNT];
		} ROS_APIC_INFO_TABLE, *PROS_APIC_INFO_TABLEtypedef struct _HALP_APIC_INFO_TABLE
{
    ULONG ApicMode;
    ULONG ProcessorCount; /* Count of all physical cores, This includes BSP */
    ULONG IOAPICCount;
    ULONG LocalApicPA;                // The 32-bit physical address at which each processor can access its local interrupt controller
    ULONG IoApicVA[HALP_APIC_INFO_TABLE_IOAPIC_NUMBER];
    ULONG IoApicPA[HALP_APIC_INFO_TABLE_IOAPIC_NUMBER];
    ULONG IoApicIrqBase[HALP_APIC_INFO_TABLE_IOAPIC_NUMBER]; // Global system interrupt base
} HALP_APIC_INFO_TABLE, *PHALP_APIC_INFO_TABLE;;
}
