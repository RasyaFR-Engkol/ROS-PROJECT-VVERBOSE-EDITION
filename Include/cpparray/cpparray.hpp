#pragma once

#include <rosval.h>

ABI_C VOID (*__init_array_start[])();
ABI_C VOID (*__init_array_end[])();

namespace InitializeFirst{
    namespace CPPArray{
        VOID InitializeClassArray();
    }
}