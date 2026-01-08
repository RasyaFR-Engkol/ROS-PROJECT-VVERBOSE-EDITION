/* Minimal TSS interface for ROS */
#pragma once

#include <rosval.h>

namespace TSS {
    // Initialize TSS and install into GDT. `rsp0_top` is the virtual address
    // of the top of the kernel stack (RSP value to use on ring transition).
    void Init(UPTR rsp0_top);

    // Update RSP0 at runtime (per-cpu use)
    void SetRsp0(UPTR rsp0_top);

    struct __attribute__((packed)) _KTSS64 {
        U32 reserved0;
        U64 rsp0;
        U64 rsp1;
        U64 rsp2;
        U64 reserved1;
        U64 ist1;
        U64 ist2;
        U64 ist3;
        U64 ist4;
        U64 ist5;
        U64 ist6;
        U64 ist7;
        U64 reserved2;
        U16 reserved3;
        U16 iomap_base;
    };

    VOID InitializeForCurrentCPU();
    VOID UpdateRSP0(U64 NewStackPointer);
}
