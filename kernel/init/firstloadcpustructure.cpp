#include <rosval.h>
#include <dispatch/idt.hpp>
#include <../firmware/chipset/chipset.hpp>
#include <global/global.hpp>

namespace InitializeFirst{
    namespace CPUStructure{
        VOID InitializeCPUStructure(U64 &OutProccessorNumber){
            Firmware::Chipset::InitializeCurrentCPU(&KGlobal::CPUGlobal);
            TSS::InitializeForCurrentCPU();
            KGlobal::CPUCount++;
            IDT::InitializeIDT();

            OutProccessorNumber = Firmware::Chipset::GetInitialApicId();
        }
    }
}