#include "Include/rossys.hpp"
#include "kernel/mm/mm.hpp"
#include <rosval.h>
#include <stddef.h>
#include <stdint.h> // Untuk 'uint64_t'
#include <../firmware/chipset/chipset.hpp>
#include <../kernel/dev/devicemanager.hpp>
#include <multiboot.h>
#include <cpparray/cpparray.hpp>
#include <global/global.hpp>

// 1. Deklarasi label dari linker.ld (sekarang 64-bit pointers)

// 2. Deklarasi 'kernel_main' C++
ABI_C void KernelPhaseMin1();

// 3. Fungsi 'lem' 64-bit
// Argumen 'mb_info_ptr' ini datang dari register 'rdi'
// (yang kita 'mov rdi, rbx' di asm)
ABI_C void KernelEntryPoint(uint64_t mb_info_ptr) {
    KGlobal::BootCycleFirst = Arch::ASM::RdTSC();
    KernelIntuitive::Multiboot::ParseMultiboot2Info(mb_info_ptr);

    MmSetupPaging();

    // Panggil Global Constructor C++ (now executing on high kernel stack)
    InitializeFirst::CPPArray::InitializeClassArray();

    // Panggil Kernel C++ 64-bit
    KernelPhaseMin1();

    // Hang
    for (;;) {
        asm("hlt");
    }
}