/* Minimal 64-bit TSS implementation
 * - Builds a 64-bit TSS descriptor in the GDT (two 8-byte entries)
 * - Loads the TR (ltr) with the selector
 * - Provides runtime update for rsp0
 */

#include "tss.hpp"
#include "string.hpp"
#include <serial.hpp>
#include <rosval.h>
#include <../firmware/chipset/chipset.hpp>
#include <logging.hpp>

using namespace Serial;

// GDT selector index to place TSS (two consecutive entries).
static constexpr unsigned TSS_GDT_INDEX = 6;

struct TSSDescriptor {
    U16 LimitLow;
    U16 BaseLow;
    U8  BaseMiddle;
    U8  Access;      // 0x89
    U8  Granularity; // 0x00
    U8  BaseHigh;
    U32 BaseUpper;
    U32 Reserved;
} __attribute__((packed));

STATIC VOID SetupTSSDescriptor(TSSDescriptor *Desc, U64 Base, U32 Limit){
    Desc->LimitLow    = Limit & 0xFFFF;
    Desc->BaseLow     = Base & 0xFFFF;
    Desc->BaseMiddle  = (Base >> 16) & 0xFF;
    Desc->Access      = 0x89; // Present, Ring 0, Type 9 (64-bit TSS)
    Desc->Granularity = 0x00;
    Desc->BaseHigh    = (Base >> 24) & 0xFF;
    Desc->BaseUpper   = (Base >> 32) & 0xFFFFFFFF;
    Desc->Reserved    = 0;
}

namespace TSS {

    VOID InitializeForCurrentCPU(){
        auto *CurrentCPU = Firmware::Chipset::GetCurrentCpu();

        auto *NewTSS = new _KTSS64();
        String::Memset(NewTSS, 0, sizeof(_KTSS64));

        NewTSS->iomap_base = sizeof(_KTSS64);
        CurrentCPU->TssBase = NewTSS;

        if(!CurrentCPU->GdtBase){
            Printk::Panic("TSS_FAILED");
        }

        SetupTSSDescriptor(
            (TSSDescriptor*)((U64)CurrentCPU->GdtBase + (TSS_GDT_INDEX * 8)), 
            (U64)NewTSS,
            sizeof(_KTSS64) - 1
        );

        U16 Selector = TSS_GDT_INDEX * 8;
        Arch::ASM::WriteTR(Selector);

        Serial::Printf("[ROS] TSS Loaded for CPU %d at %p (GDT Index %d)\n", 
            CurrentCPU->ProcessorIndex, NewTSS, TSS_GDT_INDEX);
    }
    // C-accessible getter for the current RSP0 value. Useful for assembly
    // stubs that need to switch to the kernel stack for syscall handling.
    VOID UpdateRSP0(U64 NewStackPointer){
        Firmware::Chipset::GetCurrentCpu()->TssBase->rsp0 = NewStackPointer;
    }

} // namespace TSS
