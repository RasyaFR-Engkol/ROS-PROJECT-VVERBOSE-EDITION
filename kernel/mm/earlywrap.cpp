#include "mm.hpp"

extern "C" void Paging_Initialize_C() {
    MemoryManager::Paging::Initialize();
}
extern "C" void Paging_RelocateGDT_C() {
    MemoryManager::Paging::RelocateGDTToHigh();
}
extern "C" void Paging_SwitchStack_C() {
    // Switch to new kernel stack and finalize (TSS init + disable low-half)
    MemoryManager::Paging::SwitchToKernelStack(8);
}

ABI_C VOID MmSetupPaging(){
    MemoryManager::Paging::InitAllocators();

    UPTR NewPML4 = MemoryManager::Paging::BuildKernelPageTable();

    MemoryManager::Paging::ActivatePaging(NewPML4);

    MemoryManager::Paging::PostPagingInit();
}
