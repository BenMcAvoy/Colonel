#pragma once

#include <cstdint>
#include "driver.h"

namespace SDK {
    class UWorld;
    class FNamePool;

    extern ::DriverManager dm;
    extern uintptr_t imgBase;
    extern UWorld* GWorld;
    extern FNamePool* GNamePool;
}
