// Minimal kernel entry implementation that prints to serial (COM1)
// This helps verify that we've actually jumped into 64-bit kernel_main.
#include "Include/spinlock/simple.hpp"
#include "dispatch/idt.hpp"
#include "firmware/chipset/chipset.hpp"
#include "global/global.hpp"
#include "kernel/dev/devicemanager.hpp"
#include "kernel/hal/hal.hpp"
#include "x86_64/tss.hpp"
#define PRINTK_MODULE_NAME "Kernel"
#include <stdint.h>
#include <serial.hpp>
#include <rosval.h>
#include <bootinfo.h>
#include "firmware/acpi/acpi.hpp"
#include "firmware/acpi/driver/timer/timer.hpp"
#include "framebuffer.hpp"
#include "kernel/driver/e1000/e1000.hpp"
#include "kernel/driver/pci/pci.hpp"
#include "kernel/driver/ahci/ahci.hpp"
#include "kernel/driver/xhci/xhci.hpp"
#include "kernel/driver/pic/pic.hpp"
#include "kernel/driver/pic/timer/pit.hpp"
#include "kernel/filesys/gpt/gpt.hpp"
#include "kernel/filesys/rootfs/rootfs.hpp"
#include "kernel/filesys/vfs/vfs.hpp"
#include "kernel/filesys/fat32/fat32.hpp"
#include "kernel/filesys/devfs/devfs.hpp"
#include "kernel/intidt/idt.hpp"
#include "kernel/log/fbcon/fbcon.hpp"
#include "kernel/driver/fb/fbcon_driver.hpp"
#include "kernel/log/printk/printk.hpp"
#include "kernel/mm/kmalloc/kmalloc.hpp"
#include "kernel/mm/mm.hpp"
#include "firmware/acpi/madt/smpmod/smp.hpp"
#include "debug.hpp"
#include "kernel/mm/shm/shm.hpp"
#include "rossys.hpp"
#include <filesystem/filesystem.hpp>
#include "kernel/filesys/pmos/partition_manager.hpp"
#include <string.hpp>
#include <userland/syscall.hpp>
#include "kernel/task/task.hpp"
#include "rostime.hpp"
#include <network/ethernet.hpp>
#include <network/dns.hpp>
#include <network/tcp.hpp>
#include <network/udp.hpp>
#include <network/dhcp.hpp>
#include <initializefirst.hpp>
#include <hal.hpp>

VOID IdleLoop(){
    while(TRUE){
        Arch::ASM::HaltCPU();
    }
}

STATIC Arch::Spinlock::Spinlock KernelIntuitiveFreezeExecutionLock;

ABI_C NORET void KernelPhaseMin1()
{ 
    U32 CPUID;
    U64 ProccessorNumber;

    InitializeFirst::CPUStructure::InitializeCPUStructure(ProccessorNumber);

    // 1. Setup Process (Wadah)
    Tasking::KProccess *SystemProccess = new Tasking::KProccess();
    String::Memset(SystemProccess, 0, sizeof(Tasking::KProccess));

    SystemProccess->ProccessID = 4; // PID 4 = System
    String::Strcpy(SystemProccess->ImageName, "System");
    // PENTING: Pakai CR3 yang sedang aktif
    SystemProccess->DirectoryTableBase = (CR3)MemoryManager::DoCR3::GetCurrentCR3(); 
    SystemProccess->Active = TRUE;

    // 2. Setup Thread (Pekerja)
    Tasking::KThread *InitThread = new Tasking::KThread();
    String::Memset(InitThread, 0, sizeof(Tasking::KThread));

    InitThread->Proccess = SystemProccess;
    InitThread->State = Tasking::KThread::RUNNING; // Karena sedang dieksekusi
    InitThread->Priority = 31; // Prioritas tertinggi biar gak gampang di-kick
    InitThread->BasePriority = 31; 
    
    // PENTING: Quantum (Jatah Waktu)
    // Kalau 0, nanti pas timer interrupt pertama, dia langsung dianggap "habis waktu"
    // dan scheduler bakal panik nyari pengganti (padahal belum ada thread lain).
    InitThread->Quantum = 10; // Kasih jatah 10 tick

    InitThread->SystemThread = TRUE; // Ini thread kernel
    InitThread->ThreadID = 1; // TID 1 (atau 0)

    // 3. Setup Stack
    // Kita alokasi stack baru, TAPI saat ini CPU masih pakai stack lama (boot stack).
    // Ini gapapa, nanti akan sinkron sendiri pas context switch pertama.
    U64 StackSize = 16 * 1024;
    POINTER StackPtr = Kmalloc::Alloc(StackSize);

    InitThread->StackBase = (U64)StackPtr;
    InitThread->StackLimit = (U64)StackPtr + StackSize;
    InitThread->KernelStackPointer = InitThread->StackLimit;

    TSS::UpdateRSP0(InitThread->StackBase);

    // 4. Linkage
    SystemProccess->ThreadListHead = InitThread;
    
    // 5. Registrasi Global (PENTING)
    // Supaya fungsi kayak GetCurrentThread() jalan
    Firmware::Chipset::GetCurrentCpu()->CurrentThread = InitThread;
    Firmware::Chipset::GetCurrentCpu()->CurrentProccess = SystemProccess;

    CPUID = Firmware::Chipset::GetInitialApicId();

    KernelIntuitiveFreezeExecutionLock.Acquire();

    // setup  HAL
    HAL::InitializeHal(ProccessorNumber);

    // set prosesor aktif

    // rilis kunci
    KernelIntuitiveFreezeExecutionLock.Release();

    HAL::IRQL::RaiseIRQL(HIGH_LEVEL);

    if (Firmware::Chipset::IsBootstrapProcessor()) Userland::Syscall_Init();

    Firmware::CPU::InitializeXState();
    
    if (!DeviceManager::ObjectInstance.FirstInitializeDevOBJManager()) {
        // KernelRandez::BUG::CallBugCheck();
    }

    // Deteksi Chipset 
    Firmware::Chipset::DetectMotherboardChipset();
    PIC::InitializePIC();
    PIC::Keyboard::InitializeKeyboardPIC();
    PIT::InitializePIT(100); // set PIT to 100 Hz (calibration/reference)
    // Enable IRQ-driven serial input (COM1 IRQ4)
    Serial::EnableIRQInput();

    // Enable interrupts so PIT IRQs will increment PIT::ticks for calibration
    Arch::Sti();

    // Calibrate and start LAPIC timer at 100 Hz (uses PIT ticks)
    // Use a distinct vector for LAPIC timer (not IRQ0/PIT vector 0x20)
    // to avoid conflicts with the PIT handler while calibration runs.
    ACPI::Timer::InitializeLapicTimer(CONFIG_TIMER_HEXA_GLOBAL, 100, TRUE);

    // Now mask and disable legacy PIC hardware while interrupts are briefly
    // disabled inside the call. After that, re-enable interrupts so LAPIC
    // delivered interrupts are accepted.
    PIC::DisableIRQWhileAndMaskOldPIC();
    Arch::Sti();

    // Now that LAPIC timer calibrated, PIT ticks flowing, interrupts enabled,
    // and IOAPIC/LAPIC initialized, start Application Processors.
    ACPI::LAPIC::SMP::InitSMP();

    BootInfoPrint();

    ROOTFS::InitROOTFS();
    DEVFS::Init();
    FB::Init();
    Printk::Init();

    // Initialize PCI and its drivers
    PCI::IntializePCIDrivers();

    // xHCI test: send multiple Enable Slot commands (no NOOP, no polling) to verify repeated MSIs.
    //xHCI::InterruptBurstTest(5); Disable this for now to reduce noise.

    // AHCI read test: try LBA0 from first available SATA port and hex dump
    //AHCI::TestReadLBA0();

    // Mount an in-kernel DevFS at /dev and register framebuffer device there

    GPTFS::InitFs();

    Userland::Syscall_Init();
    SharedMemoryManager::Init();

    Tasking::CreateIdleTask(IdleLoop);

    {
        File *F = VFSManager::Open("/init.elf", O_RDONLY);
        if(F){
            U64 ELFSize = F->FileSize;
            VOID *ELFImage = Kmalloc::Alloc(ELFSize);
            if(!ELFImage){
                Printk::Write(Printk::Level::LOG_ERR, "KernelMain: failed to allocate memory for init.elf\n");
                goto halt;
            }
            U64 ReadBytes = VFSManager::Read(F, (U8*)ELFImage, ELFSize);
            if(ReadBytes != ELFSize){
                Printk::Write(Printk::Level::LOG_ERR, "KernelMain: failed to read full init.elf (read %llu of %llu)\n",
                              (unsigned long long)ReadBytes, (unsigned long long)ELFSize);
                Kmalloc::Free(ELFImage);
                goto halt;
            }

            Tasking::CreateUserTask("init", ELFImage);
            Kmalloc::Free(ELFImage);
            VFSManager::Close(F);
        } else {
            Printk::Write(Printk::Level::LOG_ERR, "KernelMain: failed to open /mnt/part1/init.elf\n");
        }
    }
    
    Tasking::SchedulerStart();

    UNUSED__ halt:
    
    // Main idle loop: poll serial and keyboard consumers so IRQ-driven
    // producers are serviced. This keeps IRQ handlers minimal (they only
    // enqueue) while the main loop does I/O and console rendering.
    for (;;) {
        FBConsole::UpdateCursor();
        // Drain any incoming serial characters (mirrors to FB/serial)
        Serial::PollToConsoles();
        // Process queued keyboard scancodes and echo them to consoles
        PIC::Keyboard::Poll();
        // Halt until next interrupt to reduce CPU usage
        Arch::ASM::HaltCPU();
    }
    
}
