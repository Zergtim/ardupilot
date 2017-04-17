#include <stdarg.h>
#include <stdio.h>

#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/system.h>

#include "io.h"

extern const AP_HAL::HAL& hal;

namespace AP_HAL {

void init()
{
}

void panic(const char *errormsg, ...)
{
    va_list ap;

    va_start(ap, errormsg);
    vdprintf(1, errormsg, ap);
    va_end(ap);
//    write(1, "\n", 1);

    hal.scheduler->delay_microseconds(10000);
//    _px4_thread_should_exit = true;
    exit(1);
}

uint32_t micros()
{
    return micros64() & 0xFFFFFFFF;
}

uint32_t millis()
{
    return millis64() & 0xFFFFFFFF;
}

uint64_t micros64()
{
    uint64_t nowclocks;
    __asm__ __volatile__ ("rdtsc" : "=A"(nowclocks) );
    return nowclocks/vx86_CpuCLK();
}

uint64_t millis64()
{
    return micros64() / 1000;
}

} // namespace AP_HAL
