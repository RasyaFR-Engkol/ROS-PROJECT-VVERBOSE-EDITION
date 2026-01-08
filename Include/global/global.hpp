#pragma once

#include "dispatch/idt.hpp"
#include <rosval.h>
#include <task.hpp>
#include <../x86_64/tss.hpp>

namespace KGlobal{
    extern TSC BootCycleFirst;

    typedef struct _CPUBlock {
        // --- 1. IDENTIFICATION (Static Info) ---
        // Hasil CPUID Leaf 0 & 0x8000000x
        struct _CPUBlock *Self;
        CHAR8 VendorID[13];          // "GenuineIntel" atau "AuthenticAMD"
        CHAR8 BrandString[49];       // "Intel(R) Core(TM) i9-13900K..."
        
        // Hasil CPUID Leaf 1 (Version Info)
        U32 Family;
        U32 Model;
        U32 Stepping;
        U32 ProcessorType;           // OEM, Retail, Engineering Sample?

        // --- 2. HARDWARE TOPOLOGY (Penting buat Multicore) ---
        U32 ApicID;                  // ID Hardware Asli (dari Local APIC)
        U32 ProcessorIndex;          // Index Software (CPU 0, CPU 1, ...)

        // --- 3. FEATURES (Capabilities) ---
        // Enaknya pake Union biar bisa akses Raw atau per-bit
        union {
            U32 Value;
            struct {
                U32 HasFPU    : 1; // Floating Point Unit
                U32 HasVME    : 1; // Virtual Mode Extension
                U32 HasDE     : 1; // Debugging Extension
                U32 HasPSE    : 1; // Page Size Extension (4MB Pages)
                U32 HasTSC    : 1; // Time Stamp Counter
                U32 HasMSR    : 1; // Model Specific Registers
                U32 HasPAE    : 1; // Physical Address Extension
                U32 HasMCE    : 1; // Machine Check Exception
                U32 HasCX8    : 1; // CMPXCHG8B Instruction
                U32 HasAPIC   : 1; // On-chip APIC Hardware
                U32 HasSEP    : 1; // Fast System Call (SYSENTER/SYSEXIT)
                U32 HasMTRR   : 1; // Memory Type Range Registers
                U32 HasPGE    : 1; // Page Global Enable
                U32 HasMCA    : 1; // Machine Check Architecture
                U32 HasCMOV   : 1; // Conditional Move Instruction
                U32 HasPAT    : 1; // Page Attribute Table
                U32 HasPSE36  : 1; // 36-bit PSEs
                U32 HasPSN    : 1; // Processor Serial Number
                U32 HasCLFSH  : 1; // CLFLUSH Instruction
                U32 HasSSE : 1;
                U32 HasSSE2 : 1;
                U32 HasXSAVE : 1;
                U32 Reserved  : 13;
            } Bits;
        } FeatureFlags;
        
        // Fitur Tambahan (Leaf 1 ECX & Leaf 7)
        BOOL HasSSE3;
        BOOL HasAVX;
        BOOL HasXSAVE; 

        // --- 4. SCHEDULER STATE (Jantungnya OS) ---
        // Pointer ke Thread yang SEDANG jalan di Core ini
        struct Tasking::KThread* CurrentThread; 
        
        // Pointer ke Thread yang AKAN jalan selanjutnya (Optimization)
        struct Tasking::KThread* NextThread;  
        
        // Thread "Gabut" khusus Core ini (dijalankan kalau gak ada kerjaan)
        struct Tasking::KThread* IdelThread; 

        struct Tasking::KProccess* CurrentProccess;

        // --- 5. ARCHITECTURE STATE (Low Level) ---
        // Struktur data x86/x64 yang WAJIB unik per core
        void* GdtBase;       // Pointer ke GDT array core ini
        void* IdtBase;       // Pointer ke IDT array core ini
        TSS::_KTSS64* TssBase;       // Pointer ke Task State Segment core ini
        
        // Pointer ke struktur PCR (Processor Control Region) 
        // Di x64 biasanya disimpen di GS Base
        struct _KPCR* Pcr;   

        // --- 6. PERFORMANCE & DEBUG ---
        U64 CycleCount;      // Total cycle berjalan (Update tiap tick)
        U32 MHz;             // Kecepatan clock terdeteksi
        U32 InterruptCount;  // Statistik: Berapa kali core ini diganggu?
        U32 StallScaleFactor;

        __IRQL CurrentIRQL;
        U64 KernelStackByCPU;

        alignas(16) IDTEntry IDT[256];

    } CPUBlock;

    extern CPUBlock CPUGlobal; //FIXME: Ga boleh 1 variable aja
    extern INTN CPUCount;
    extern U32 ProccessorToApicIDTableIndex[16];
    static U32 g_XStateSize = 512; // Default Legacy (FxSave)

    extern Tasking::KProccess *CurrentProccess;
    extern Tasking::KThread *CurrentThread;
}
