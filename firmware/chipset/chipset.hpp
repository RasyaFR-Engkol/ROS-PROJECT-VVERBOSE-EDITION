#pragma once

#define PCI_VENDOR_INTEL    0x8086
#define PCI_DEVICE_PIIX3    0x7000
#define PCI_DEVICE_ICH9     0x2918

#include <rosval.h>
#include <global/global.hpp>

/*
 * Chipset.hpp:
 * Fungsionalitas:
 * Digunakan untuk mencari PIN Chipset Type. Bisa PIIX3, ICH9, dll.
 * 
 * Untuk menentukan apakah kita harus menggunakan mode legacy atau mode modern
 * dalam mengakses perangkat-perangkat tertentu seperti PIC, RTC, dsb. 
 */

namespace Firmware{
    ABI_C {
        namespace Chipset{
            enum ChipsetType{
                CHIPSET_UNKNOWN = 0,
                CHIPSET_PIIX3,
                CHIPSET_ICH9
            };

            enum CpuFeatureId {
                FEAT_FPU,
                FEAT_APIC, // Penting buat HAL nentuin pake PIC/APIC
                FEAT_TSC,  // Penting buat Timer presisi
                FEAT_SSE,
                FEAT_SSE2,
                FEAT_SSE3,
                FEAT_AVX,
                FEAT_XSAVE,
                FEAT_MTRR
            };

            extern ChipsetType g_DetectedChipset;

            ChipsetType DetectMotherboardChipset();
            VOID DetectCPU(KGlobal::CPUBlock *CPUBLOCK);
            VOID InitializeFeatures(KGlobal::CPUBlock *CPUBLOCK);
            BOOL IsBootstrapProcessor();
            U8 GetProcessorLogicalId();
            U8 GetInitialApicId();
            BOOL IsHasFeature(CpuFeatureId Feature);
            VOID InitializeCurrentCPU(KGlobal::CPUBlock *NewCPUBlock);

            inline KGlobal::CPUBlock* GetCurrentCpu() {
                KGlobal::CPUBlock* pBlock;
                
                // Kita ambil nilai dari GS:[0]. 
                // Karena GS Base nunjuk ke awal struct, dan awal struct adalah pointer 'Self',
                // maka kita dapet alamat memori struct itu sendiri.
                __asm__ __volatile__ (
                    "movq %%gs:0x00, %0" 
                    : "=r"(pBlock) 
                    : /* no input */ 
                    : "memory"
                );
                
                return pBlock;
            }
        }

        namespace CPU{
            VOID InitializeXState();
        }
    }
}