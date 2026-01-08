#pragma once

// task.hpp
// 
// Ini untuk task scheduler pertama kita yang berbasis
// round-robin. Task scheduler ini akan mengatur penjadwalan
// task-task yang berjalan di sistem operasi kita.
//
// Note: Ini Task Control Block (TCB) yang sangat sederhana.
// Di implementasi nyata, TCB biasanya memiliki banyak
// informasi tambahan seperti prioritas, status task, dan
// informasi konteks CPU lainnya.

#include "rosval.h"
#include <cpu_context.hpp>
// Forward-declare File to avoid heavy include and circular dependencies
class File;

// VMA protection flags (used in `VMArea::Prot`)
#define VMA_READ   (1ULL << 0)
#define VMA_WRITE  (1ULL << 1)
#define VMA_EXEC   (1ULL << 2)
#define VMA_SHARED (1ULL << 3)

// Mapping flags (used in `VMArea::Flags` / mmap-like interfaces)
#define MAP_SHARED    (1ULL << 0)
#define MAP_PRIVATE   (1ULL << 1)
#define MAP_FIXED     (1ULL << 2)
#define MAP_ANONYMOUS (1ULL << 3)
#define MAP_STACK     (1ULL << 4) // Hint: mapping is intended for thread/user stack

#define MAX_HANDLE 64
#define MLFQ_LEVELS 4
#define PRIORITY_BOOST_INTERVAL 1000

#define TIMER_WHEEL_SIZE 512 // Harus power of 2

// Ini harusnya adjustable terhadap kebutuhan sistem operasi kita
// tapi untuk sekarang kita tetapkan 256 Task saja.
#define MAX_TASK 256

// FLAGS MLFQ
#define TASK_SLEEPING (1 << 0)

// flags POLLIN
// Konstanta bitmask poll (Standar)
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020

extern VOLATILE U32 PriorityBitmap;

namespace Tasking {
    constexpr U64 PID_IDLE = 0;
    constexpr U64 PID_INIT = 1;
    constexpr U64 PID_USER_START = 10; // User process mulai dari sini
}

extern VOLATILE U64 GlobalBoostEpoch;

// tasking FLAGS
#define PERM_ESSENTIAL_SYSTEM   (1 << 1) // Gak bisa di-kill sembarangan
#define PERM_ADMIN_SUDO         (1 << 2) // Punya akses root/kernel space helper
#define PERM_SYS_CRITICAL       (1 << 3) // SCHEDULER: Selalu Priority 0, Anti-Turun Kasta

namespace Tasking{
    struct MM_VAD {
        U64 StartVpn;   // Starting Virtual Page Number
        U64 EndVpn;     // Ending Virtual Page Number
        U32 Flags;      // Protection (RWX)
        MM_VAD *Next;   // Tetap list dulu gapapa
    };

    struct KProccess{
        PID_T ProccessID;
        PID_T ParentID;
        ANSI_STRING ImageName[32];

        struct MM_VAD *VadRoot;
        U64 NextFreeAddress;

        CR3 DirectoryTableBase;

        RHANDLE HandleTable[MAX_HANDLE];
        struct KThread *ThreadListHead;

        U32 ExitStatus;
        BOOL Active;
    };

    struct KThread{
        enum State {
            INITIALIZED,
            READY,
            RUNNING,
            STANDBY,    // Siap lari, cuman nunggu Core kosong
            TERMINATED,
            WAITING,    // Tidur (Sleep, Wait for IO, Wait for Mutex)
            TRANSITION  // Page Fault / Stack Swapped Out (Advanced)
        } State;

        U8 Priority;
        U8 BasePriority;
        U64 Quantum;

        U64 KernelStackPointer;

        U64 StackBase;
        U64 StackLimit;

        U8 *FPURegion;

        struct KProccess *Proccess;

        KThread *NextReadyQueue;
        KThread *NextInProccess;

        U64 SleepUntil;
        U32 WaitReason;
        long RSTATUS;

        U32 ThreadID;
        BOOL SystemThread;
        BOOL YieldRequested;

        VOID *TrapFrame;
    };

    // Variabl global untuk task management
    extern U64 ActiveTask;
    extern U64 CurrentTaskIndex;
    extern VOLATILE BOOL SchedulerActive;
    extern U64 g_ForegroundPID;
    extern VOLATILE BOOL ForceReschedule;

    VOID SchedulerStart();
    VOID CreateKThread(VOID (*Entry)(VOID));
    VOID CreateUserTask(const CHAR8 *Name, VOID *ELFImage);
    VOID SchedulerTick(void *context);
    U64 GetTimeSliceForPriority(U8 Priority);
    U64 GetTimeAllotmentForPriority(U8 priority);
    VOID SchedulerYield();
    U64 CreateUserAddressSpace();
    U64 CloneUserAddressSpace(U64 SourceCR3);
    VOID FreeUserAddressSpace(U64 cr3); // Expose this
    // Return a copy of the currently running Task struct. If no current
    // task exists, returns a zeroed Task with pid==0.
    // Faster accessor returning pointer to the current Task (or nullptr)
    // Use this when you need to access fields like CR3 without copying.
    // Set a signal on a task or a process group. If `isGroup` is TRUE,
    // `id` is interpreted as a PGID and the signal is delivered to all
    // members of that group. `signal` is the POSIX signal number
    // (e.g., 2 for SIGINT).
    VOID SetTaskSignal(U64 id, U32 signal, BOOL isGroup);
    VOID CreateIdleTask(VOID (*Entry)(VOID));
    short CheckFileDesc(int fd, short events);
    VOID ReapDTask();
    VOID Sleep(U64 ms);
    VOID Debug_DumpProcessState();
    VOID Debug_DumpFDProccessBelowPID10();
    VOID Debug_MinorAndMajorFaultsBelowPID10();
}