#include <time.h>
#include "chrono.h"

#ifdef _WIN32

double second(void) {
    return (double) clock() / (double) CLOCKS_PER_SEC; // meglio di CLK_TCK
}

#else

#include <time.h>

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/mach_time.h>
#endif

double myWallTime(void) {



#ifdef __APPLE__
static double timeConvert = 0.0;
    if (timeConvert== 0.0)
    {
        mach_timebase_info_data_t timeBase;
        mach_timebase_info(&timeBase);
        timeConvert = (double)timeBase.numer / (double)timeBase.denom / 1e9;
    }
    return mach_absolute_time() *timeConvert;
#else
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec+ 1.0e-9     *(double)ts.tv_nsec;
#endif
}

double second(void) {
    return myWallTime();
}

#endif
