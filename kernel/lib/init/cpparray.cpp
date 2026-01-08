#include <rosval.h>
#include <cpparray/cpparray.hpp>

namespace InitializeFirst{
    namespace CPPArray{
        VOID InitializeClassArray(){
            size_t count = __init_array_end - __init_array_start;
            for (size_t i = 0; i < count; i++) {
                __init_array_start[i]();
            }
        }
    }
}
