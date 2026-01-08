#include "global/global.hpp"
#define PRINTK_MODULE_NAME "IDTINIT"
#include <rosval.h>
#include "rossys.hpp"
#include <dispatch/idt.hpp>
#include "serial.hpp"
#include "string.hpp"
#include <logging.hpp>

static InterruptHandler G_Handlers[256]; // per-vector registered handlers

// Vector Bitmap for Interrupt Allocation
static U8 G_VectorBitmap[256 / 8]; // 32 bytes

inline void BmpSet(U8 vec)   { G_VectorBitmap[vec >> 3] |=  (U8)(1u << (vec & 7)); }
inline void BmpClear(U8 vec) { G_VectorBitmap[vec >> 3] &= (U8)~(1u << (vec & 7)); }
inline bool BmpTest(U8 vec)  { return (G_VectorBitmap[vec >> 3] >> (vec & 7)) & 1u; }

ABI_C VOID DivideBy0Exception();
ABI_C VOID DebugException();
ABI_C VOID NMIException();
ABI_C VOID OFException();
ABI_C VOID BoundRangeException();
ABI_C VOID DeviceNotAvailableException();
ABI_C VOID TSSException();
ABI_C VOID SegmentationNotAVAIL();
ABI_C void IsrStub_PageFault();
ABI_C VOID DoubleFAULT();
ABI_C VOID STUB();
extern "C" void* IrqStub_Table[]; // exported from irqstub.asm

#define ROS_IRQ_BASE 0x30
#define IST_PANIC    1     // Index Stack untuk Double Fault (diatur di TSS nanti)
#define IST_NMI      2     // Index Stack untuk NMI
#define IST_MCE      3     // Index Stack untuk Machine Check

namespace IDT {
    VOID MCEHandler(void* context) {
        Arch::ASM::Cli(); // Disable interrupts immediately
        // Read some useful MSRs to help debugging the Machine Check Exception.
        U64 efer = Arch::MSR::ReadEFER();
        U64 apic_base = Arch::MSR::Read(Arch::MSR::IA32_APIC_BASE);

        // Log MSR values at error level to aid post-mortem analysis.
        Printk::Write(Printk::Level::LOG_ERR,
                      "IDT: MCE detected. MSR values: EFER=0x%016llx APIC_BASE=0x%016llx\n",
                      efer, apic_base);

        // Read Machine-Check Global Capability and Status MSRs to determine
        // how many MC banks exist and the global MCG status.
        const U32 IA32_MCG_CAP = 0x179u;
        const U32 IA32_MCG_STATUS = 0x17Au;
        U64 mcg_cap = Arch::MSR::Read(IA32_MCG_CAP);
        U64 mcg_status = Arch::MSR::Read(IA32_MCG_STATUS);
        U32 bank_count = (U32)(mcg_cap & 0xFFu);
        if (bank_count == 0) bank_count = 0; // explicit for clarity
        if (bank_count > 64) bank_count = 64; // sanity cap

        Printk::Write(Printk::Level::LOG_ERR,
                      "IDT: IA32_MCG_CAP=0x%016llx banks=%u IA32_MCG_STATUS=0x%016llx\n",
                      mcg_cap, bank_count, mcg_status);

        // Iterate each bank and print STATUS/ADDR/MISC when non-zero.
        for (U32 bank = 0; bank < bank_count; ++bank) {
            U32 base = 0x400u + (bank * 4u);
            U64 status = Arch::MSR::Read(base + 1u); // IA32_MCi_STATUS
            if (status != 0) {
                U64 addr = Arch::MSR::Read(base + 2u);  // IA32_MCi_ADDR
                U64 misc = Arch::MSR::Read(base + 3u);  // IA32_MCi_MISC
                Printk::Write(Printk::Level::LOG_ERR,
                              "IDT: MCE bank %u: STATUS=0x%016llx ADDR=0x%016llx MISC=0x%016llx\n",
                              (unsigned)bank, status, addr, misc);
            }
        }

        // Panic-level message and halt the system.
        Printk::Write(Printk::Level::LOG_EMERG, "IDT: Machine Check Exception occurred! System halt.\n");
        while (1) { asm volatile("hlt"); }
    }

    VOID SetupBasicTrap(IDTEntry *LocalIDT, KERNEL_SEGMENT KernelSegment, FLAGS32 Flags){
        LocalIDT[0].Set((VOID*)DivideBy0Exception, KernelSegment, Flags);
        LocalIDT[0x01].Set((VOID*)DebugException, KernelSegment, Flags);
        LocalIDT[0x02].Set((VOID*)NMIException, KernelSegment, Flags);
        LocalIDT[0x04].Set((VOID*)OFException, KernelSegment, Flags);
        LocalIDT[0x05].Set((VOID*)BoundRangeException, KernelSegment, Flags);
        LocalIDT[0x07].Set((VOID*)DeviceNotAvailableException, KernelSegment, Flags);
        LocalIDT[0x0A].Set((VOID*)TSSException, KernelSegment, Flags);
        LocalIDT[0x0B].Set((VOID*)SegmentationNotAVAIL, KernelSegment, Flags);

        LocalIDT[14].Set((VOID*)IsrStub_PageFault, KernelSegment, Flags);
        LocalIDT[13].Set((VOID*)IsrStub_GPFault, KernelSegment, Flags);
        LocalIDT[0x12].Set((VOID*)MCEHandler, KernelSegment, Flags);
    }

    VOID InitializeIDT() {
        String::Memset(G_VectorBitmap, 0, sizeof(G_VectorBitmap));
        String::Memset(G_Handlers, 0, sizeof(G_Handlers));
        
        IDTEntry *LocalIDT = KGlobal::CPUGlobal.IDT;

        U16 KernelCS = 0x08;
        U8 TrapFlags = IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE_TRAP;
        U8 IntFlags = IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE_INTERRUPT;

        for(U32 i = 0; i < 256; i++){
            LocalIDT[i].Set((POINTER)STUB, KernelCS, TrapFlags);
        }

        IDT::SetupBasicTrap(LocalIDT, KernelCS, TrapFlags);

        LocalIDT[0x08].Set((VOID*)DoubleFAULT, KernelCS, TrapFlags, IST_PANIC);

        // NMI (#NMI) - Vector 2
        LocalIDT[0x02].Set((VOID*)NMIException, KernelCS, TrapFlags, IST_NMI);

        // Machine Check (#MC) - Vector 18 (0x12)
        LocalIDT[0x12].Set((VOID*)MCEHandler, KernelCS, TrapFlags, IST_MCE);

        for (U32 i = 0; i < 208; ++i) {
            // Cek overflow vector biar gak nulis di luar array 256
            if ((ROS_IRQ_BASE + i) > 255) break;

            void* stub = ((void**)IrqStub_Table)[i];
            LocalIDT[ROS_IRQ_BASE + i].Set(stub, KernelCS, IntFlags);
        }

        struct __attribute__((packed)) {
            U16 limit;
            UPTR base;
        } IDTR;

        // Limit harus ukuran byte array dikurang 1
        IDTR.limit = (sizeof(IDTEntry) * 256) - 1;
        // Base harus menunjuk ke IDT milik CPU ini
        IDTR.base = (UPTR)LocalIDT;
        
        asm volatile("lidt %0" : : "m"(IDTR));
    }

    // Correctly spelled API
    void RegisterInterruptHandler(U8 vector, InterruptHandler GHandler) {
        // vector is U8 (0..255) so the range check is unnecessary
        G_Handlers[vector] = GHandler;
    }

    // Back-compat for earlier misspelling
    void RegsiterInterruptHandler(U8 vector, InterruptHandler GHandler) {
        RegisterInterruptHandler(vector, GHandler);
    }

    // Invoke if present
    void InvokeInterruptHandler(U8 vector, void *context) {
        // vector is U8 (0..255) so the range check is unnecessary
        auto fn = G_Handlers[vector].handler;
        if (fn) fn(context);
    }

    U8 AllocateVector(){
        for(VAL32 V = 0x84; V < 0xF0; V++){
            if (!BmpTest(V)) {
                BmpSet(V);
                return (U8)V;
            }
        }
        Printk::Write(Printk::Level::LOG_ERR, "IDT: No free interrupt vectors available for allocation!\n");
        return 0;
    }

    VOID FreeVector(U8 vector){
        if (vector >= 0x84) { // Jangan free vektor hardware
            BmpClear(vector);
            G_Handlers[vector].handler = nullptr;
        }
    }
}