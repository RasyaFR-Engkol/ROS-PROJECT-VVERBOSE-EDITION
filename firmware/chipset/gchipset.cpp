#include "chipset.hpp"
#include "global/global.hpp"
#include <../kernel/driver/pci/pci.hpp>
#define PRINTK_MODULE_NAME "CHIPSET"
#include <logging.hpp>
#include <../kernel/hal/hal.hpp>

namespace Firmware{
    ABI_C {
        namespace Chipset{
            using namespace KGlobal;
            ChipsetType g_DetectedChipset = CHIPSET_UNKNOWN;

            BOOL IsHasFeature(CpuFeatureId Feature) {
                // Ambil pointer ke struct global yang sudah diisi oleh DetectCPU()
                KGlobal::CPUBlock *CPU = &KGlobal::CPUGlobal;

                switch (Feature) {
                    // --- Cek dari Union FeatureFlags (Leaf 1 EDX) ---
                    case FEAT_FPU:  return CPU->FeatureFlags.Bits.HasFPU;
                    case FEAT_APIC: return CPU->FeatureFlags.Bits.HasAPIC;
                    case FEAT_TSC:  return CPU->FeatureFlags.Bits.HasTSC;
                    case FEAT_MTRR: return CPU->FeatureFlags.Bits.HasMTRR;
                    case FEAT_SSE:  return CPU->FeatureFlags.Bits.HasSSE;
                    case FEAT_SSE2: return CPU->FeatureFlags.Bits.HasSSE2;
                    
                    // --- Cek dari Boolean Terpisah (Leaf 1 ECX & Extended) ---
                    case FEAT_SSE3: return CPU->HasSSE3;
                    case FEAT_AVX:  return CPU->HasAVX;
                    case FEAT_XSAVE: return CPU->HasXSAVE;

                    default: return FALSE;
                }
            }

            ChipsetType DetectMotherboardChipset(){
                    for (U8 dev = 0; dev < 32; dev++) {
                        U32 vendDev = PCI::ReadDword(0, dev, 0, 0x00);
                        U16 vendor = vendDev & 0xFFFF;
                        U16 device = (vendDev >> 16) & 0xFFFF;

                        if (vendor == PCI_VENDOR_INTEL) {
                            if (device == PCI_DEVICE_PIIX3) {
                                Printk::Write(Printk::Level::LOG_INFO, "[CHIPSET] Detected PIIX3 (Legacy Mode)\n");
                                return CHIPSET_PIIX3;
                            }
                            if (device == PCI_DEVICE_ICH9) {
                                Printk::Write(Printk::Level::LOG_INFO, "[CHIPSET] Detected ICH9 (Modern Mode)\n");
                                return CHIPSET_ICH9;
                            }
                        }
                    }

                    Printk::Write(Printk::Level::LOG_WARNING, "[CHIPSET] Unknown Chipset! Defaulting to PIIX3 logic.\n");
                return CHIPSET_PIIX3; // Fallback aman
            }

            inline VOID CopyRegToString(CHAR8* Dst, U32 Reg) {
                Dst[0] = (Reg >> 0) & 0xFF;
                Dst[1] = (Reg >> 8) & 0xFF;
                Dst[2] = (Reg >> 16) & 0xFF;
                Dst[3] = (Reg >> 24) & 0xFF;
            }

            VOID DetectCPU(KGlobal::CPUBlock *DetectedCPU){
                using namespace Arch::ASM;

                KGlobal::CPUBlock *CPU = DetectedCPU;
                REGISTER EAX, EBX, ECX, EDX;

                __cpuid(0, EAX, EBX, ECX, EDX);

                U32 MaxLeaf = EAX;
                CopyRegToString(&CPU->VendorID[0], EBX);
                CopyRegToString(&CPU->VendorID[4], EDX);
                CopyRegToString(&CPU->VendorID[8], ECX);
                CPU->VendorID[12] = '\0';

                if(MaxLeaf >= 1){
                    __cpuid(1, EAX, EBX, ECX, EDX);

                    CPU->Stepping = EAX & 0xf;
                    CPU->Model = (EAX >> 4) & 0xf;
                    CPU->Family = (EAX >> 8) & 0xf;
                    CPU->ProcessorType = (EAX >> 12) & 0x3;

                    if (CPU->Family == 0xF || CPU->Family == 0x6) {
                        CPU->Model += ((EAX >> 16) & 0xF) << 4;
                    }
                    if (CPU->Family == 0xF) {
                        CPU->Family += (EAX >> 20) & 0xFF;
                    }

                    CPU->FeatureFlags.Bits.HasFPU  = (EDX >> 0) & 1;
                    CPU->FeatureFlags.Bits.HasTSC  = (EDX >> 4) & 1;
                    CPU->FeatureFlags.Bits.HasAPIC = (EDX >> 9) & 1;
                    CPU->FeatureFlags.Bits.HasMTRR = (EDX >> 12) & 1;
                    CPU->FeatureFlags.Bits.HasSSE  = (EDX >> 25) & 1;
                    CPU->FeatureFlags.Bits.HasSSE2 = (EDX >> 26) & 1;

                    // ECX Flags
                    CPU->HasSSE3  = (ECX >> 0) & 1;
                    CPU->HasAVX   = (ECX >> 28) & 1;
                    CPU->HasXSAVE = (ECX >> 26) & 1;
                }

                __cpuid(0x80000000, EAX, EBX, ECX, EDX);
                U32 MaxExtLeaf = EAX;

                if(MaxExtLeaf >= 0X80000004){
                    U32 *BrandPTR = (U32*)CPU->BrandString;

                    __cpuid(0x80000002, EAX, EBX, ECX, EDX);
                    BrandPTR[0] = EAX; BrandPTR[1] = EBX; BrandPTR[2] = ECX; BrandPTR[3] = EDX;

                    __cpuid(0x80000003, EAX, EBX, ECX, EDX);
                    BrandPTR[4] = EAX; BrandPTR[5] = EBX; BrandPTR[6] = ECX; BrandPTR[7] = EDX;

                    __cpuid(0x80000004, EAX, EBX, ECX, EDX);
                    BrandPTR[8] = EAX; BrandPTR[9] = EBX; BrandPTR[10] = ECX; BrandPTR[11] = EDX;

                    CPU->BrandString[48] = '\0'; // Safety null terminator
                } else {
                    // Fallback kalau gak support (misal CPU tua banget/Emulator basic)
                    // Pakai loop manual copy string karena kita gak bisa pake strcpy library
                    CPANSI_STRING Unknown = "Unknown x86 CPU";
                    for(int i=0; i<16; i++) CPU->BrandString[i] = Unknown[i];
                }
            }

            VOID InitializeCurrentCPU(KGlobal::CPUBlock *NewCPUBlock){
                NewCPUBlock->Self = NewCPUBlock;

                NewCPUBlock->CurrentIRQL = HIGH_LEVEL;
                NewCPUBlock->CurrentThread = nullptr;
                NewCPUBlock->ProcessorIndex = 0;

                Arch::MSR::Write(MSR_GS_BASE, (U64)NewCPUBlock);

                Firmware::Chipset::DetectCPU(NewCPUBlock);

                InitializeFeatures(NewCPUBlock);
            }

            VOID InitializeFeatures(KGlobal::CPUBlock *CPUBlock){
                KGlobal::CPUBlock *CPU = CPUBlock;

                if(CPU->FeatureFlags.Bits.HasFPU){
                    Arch::ASM::FPU_Init();
                } else {
                    //BUGCHECK // FIXME
                }

                if(CPU->FeatureFlags.Bits.HasSSE){
                    Arch::ASM::EnableSSE();
                } else {
                    // Kalau OS kamu targetnya PC tua (pre-Pentium 3), 
                    // kamu harus buat fallback software emulation (Sangat Sulit).
                }

                if(CPU->HasAVX && CPU->HasXSAVE){
                    
                }
            }

            // Return initial APIC ID from CPUID leaf 1 (EBX[31:24]).
            // This is suitable to identify processor 0 vs others during early boot.
            U8 GetInitialApicId() {
                using namespace Arch::ASM;
                REGISTER EAX, EBX, ECX, EDX;
                __cpuid(1, EAX, EBX, ECX, EDX);
                return (U8)((EBX >> 24) & 0xFF);
            }

            // Logical processor ID for simple 0/1 checks. Returns 0 for BSP (APIC ID 0),
            // 1 for any other CPU. Adapt if you need the full APIC id instead.
            U8 GetProcessorLogicalId() {
                U8 apic = GetInitialApicId();
                return (apic == 0) ? 0 : 1;
            }

            // Convenience boolean: true if this is the bootstrap processor.
            BOOL IsBootstrapProcessor() {
                return GetInitialApicId() == 0;
            }
            

        }

        namespace CPU{
            VOID InitializeXState(){
                KGlobal::CPUBlock *Cpu = Chipset::GetCurrentCpu();

                if(Cpu->FeatureFlags.Bits.HasXSAVE){
                    // siapkan OXSAVE
                    U64 CR4 = Arch::ASM::ReadCR4();
                    CR4 |= (1 << 18);
                    Arch::ASM::WriteCR4(CR4);

                    // siapkan XCR0
                    U64 XCR0 = 0;
                    XCR0 |= (1 << 0);
                    XCR0 |= (1 << 1);

                    if(Cpu->HasAVX){
                        XCR0 |= (1 << 2);
                    }

                    Arch::ASM::WriteXCR0(XCR0);

                    REGISTER EAX, EBX, ECX, EDX;
                    Arch::ASM::__cpuid(0xD, EAX, EBX, ECX, EDX);

                    KGlobal::g_XStateSize = ECX;
                    Printk::Write(Printk::Level::LOG_INFO, "STATE SIZE: %d.\n", KGlobal::g_XStateSize);
                } else {
                    // Fallback ke Legacy SSE (FXSAVE)
                    // Butuh enable FXSR di CR4 (biasanya udah di init CPU)
                    KGlobal::g_XStateSize = 512; 
                }
            }
        }
    }
}