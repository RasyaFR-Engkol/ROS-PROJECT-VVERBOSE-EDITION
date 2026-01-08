#pragma once
#include <rossys.hpp>
#include <rosval.h>

// Interrupt Request Levels (IRQL) - Jantungnya HAL NT
#define PASSIVE_LEVEL 0   // Thread biasa
#define APC_LEVEL     1   // Async Procedure Call
#define DISPATCH_LEVEL 2  // Scheduler / DPC
#define DIRQL         3   // Device Interrupts (Level 3-26)
#define HIGH_LEVEL    31  // Machine Check / NMI

#define STALL_SCALE_FACTOR 100
#define ROS_MAX_KERNEL 32

typedef struct _PROCCESSORIDENTITY{
    UCHAR8 ProccesorID;
    UCHAR8 LapicID;
    BOOL ProccessorStarted;
    BOOL IsBSP;
    KGlobal::CPUBlock *CPUBLK;
} _PROCCESSORIDENTITY, *_PPROCCESSORIDENTITY;

namespace HAL{
    VOID InitializeHal(U64 ProccessorNumber);

    VOID EnableInterrupt(U32 Vector);
    VOID DisableInterrupt(U32 Vector);
    VOID EndOfInterrupt(U32 Vector);

    namespace IRQL{
        __IRQL RaiseIRQL(__IRQL NewIrql);
        VOID LowerIrql(__IRQL OldIrql);
        __IRQL GetCurrentIrql();
    }

    U64 KernelExecutiveQueryPerformanceCounter();
    VOID KernelExecutiveStallExecutionProccessor(U32 Microseconds);

    U8 ReadPortU8(U16 Port);
    U8 WritePortU8(U16 Port, U8 Value);
}