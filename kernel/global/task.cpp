#include "global/global.hpp"
#include <rosval.h>
#include <task.hpp>

namespace KGlobal{
    Tasking::KProccess *CurrentProccess = nullptr;
    Tasking::KThread *CurrentThread = nullptr;
}