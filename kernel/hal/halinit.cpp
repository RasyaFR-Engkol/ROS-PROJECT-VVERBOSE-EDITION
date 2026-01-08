#include <rosval.h>
#include "hal.hpp"
#include <logging.hpp>
#include <../firmware/chipset/chipset.hpp>

static BOOL g_UseApic = FALSE;

namespace Internal {
    extern VOID HalInitPic();
    extern VOID HalInitApic();
    extern VOID HalPicEoi(U32 Vector);
    extern VOID HalApicEoi(U32 Vector);
    extern VOID HalInitSystemTimer(); // PIT
    extern VOID pParseAcpiTables();
}

namespace HAL{
    _PROCCESSORIDENTITY HalpProccessorIdentity[ROS_MAX_KERNEL];

    VOID pSetupProccessorTable(U64 ProccessorNumber){
        KGlobal::CPUBlock *CPUBlock;

        CPUBlock = Arch::ASM::KeGetCurrentCPUPack();
        HalpProccessorIdentity[ProccessorNumber].CPUBLK = CPUBlock;
    }

    VOID pInitializeProccessor(U64 ProccessorNumber){
        if(Firmware::Chipset::IsHasFeature(Firmware::Chipset::FEAT_APIC)){
            g_UseApic = TRUE;
            Printk::Write(Printk::Level::LOG_INFO, "APIC Detected. Using APIC Mode.\n");

            if(Firmware::Chipset::IsBootstrapProcessor() && ProccessorNumber == 0){
                //Internal::pParseAcpiTables();
            }
        } else {
            g_UseApic = FALSE;
            Printk::Write(Printk::Level::LOG_INFO, "No APIC Detected\n");

            Internal::HalInitPic();
        }

        pSetupProccessorTable(ProccessorNumber);

        //ACPI::InitializeLocalAPIC(ProccessorNumber);

        //HALL::InitializingProfileData(ProccessorNumber);
    }

    VOID InitializeHal(U64 ProccessorNumber){
        Printk::Write(Printk::Level::LOG_INFO, "HAL: Initializing Hardware Layer...\n");

        // setup HAL untuk prosesor spesifik terlebih dahulu.
        HAL::pInitializeProccessor(ProccessorNumber);

        Firmware::Chipset::GetCurrentCpu()->StallScaleFactor = STALL_SCALE_FACTOR;

        if(ProccessorNumber == 0){

        }
    }

    VOID EndOfInterrupt(U32 Vector){
        if(g_UseApic){
            Internal::HalApicEoi(Vector);
        } else {
            Internal::HalPicEoi(Vector);
        }
    }

    /* TODO:
        1. Implement IRQL Persis seperti windows
        2. Implement HAL lagi Besoknya
        3. Driver kayak FAT32 dan EXT2 diarahin ke ObjectManager, VFS gada dan
        sekarang rajanya ObjectManager */
    namespace IRQL{
        inline __IRQL KeGetSoftwareIrql() {
            __MAYBE_UNUSED __IRQL irql;
            // Baca langsung dari GS:[Offset CurrentIrql]
            // Anggap offset CurrentIrql di struct CPUBlock sudah dihitung compiler
            // Tapi karena kita pake C++, kita akses via pointer Self aja biar aman
            // (Compiler akan optimasi jadi GS access kok)
            return Firmware::Chipset::GetCurrentCpu()->CurrentIRQL;
        }

        inline VOID KeSetSoftwareIrql(__IRQL NewIrql) {
            Firmware::Chipset::GetCurrentCpu()->CurrentIRQL = NewIrql;
        }

        __IRQL RaiseIRQL(__IRQL NewIrql){
            __IRQL OldIRQL;
            OldIRQL = KeGetSoftwareIrql();
            
            KeSetSoftwareIrql(NewIrql);

            if(g_UseApic){
                OldIRQL = (U8)Arch::ASM::ReadCR8();

                if(NewIrql > OldIRQL){
                    Arch::ASM::WriteCR8(NewIrql);
                }

                // Catatan: Interrupt Hardware dengan prioritas <= NewIrql 
                // sekarang di-blok oleh CPU (Pending di Local APIC).
            } else {
                // --- MODE PIC (LEGACY / FALLBACK) ---
                // PIC 8259A gak peduli sama CR8.
                // Cara paling aman dan simpel: Kalau bukan PASSIVE, matikan semua (CLI).
                
                // Kita simpan state interrupt flag (EFLAGS.IF) sebagai "OldIrql"
                // Kalau Interrupt Nyala (1) -> Kita anggap PASSIVE (0)
                // Kalau Interrupt Mati (0)  -> Kita anggap HIGH_LEVEL (31)
                
                U64 Rflags = Arch::ASM::SaveRflags();
                OldIRQL = (Rflags & 0x200) ? PASSIVE_LEVEL : HIGH_LEVEL;

                if (NewIrql > PASSIVE_LEVEL) {
                    Arch::ASM::Cli(); 
                }
            }

            return OldIRQL;
        }

        VOID LowerIrql(__IRQL OldIrql) {
            if (g_UseApic) {
                // --- MODE APIC ---
                // Langsung tulis balik nilai lama ke CR8.
                // Kalau OldIrql == 0 (Passive), interupsi yang pending tadi langsung
                // "JEDER" masuk semua saat instruksi ini selesai.
                Arch::ASM::WriteCR8(OldIrql);

            } else {
                // --- MODE PIC ---
                if (OldIrql == PASSIVE_LEVEL) {
                    Arch::ASM::Sti();
                }
                // Kalau OldIrql == HIGH_LEVEL, jangan di STI, biarin tetep mati.
            }

            KeSetSoftwareIrql(OldIrql);
        }

        __IRQL GetCurrentIrql() {
            // CALL INTERNAL FUNC
            return KeGetSoftwareIrql();
        }
    }
}