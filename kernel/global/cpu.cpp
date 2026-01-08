#include "global/global.hpp"
#include <rosval.h>

namespace KGlobal{
    CPUBlock CPUGlobal; //FIXME: Ga boleh 1 variable aja
    INTN CPUCount = -1;

    U32 ProccessorToApicIDTableIndex[16] = {};
}