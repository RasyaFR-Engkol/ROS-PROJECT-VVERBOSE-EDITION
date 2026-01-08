#include <rosval.h>
#include <../firmware/acpi/acpi.hpp>

namespace Internal{
    VOID pParseAcpiTables(){
        ACPI::Initialize();
    }
}