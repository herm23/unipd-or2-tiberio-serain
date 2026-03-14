#include <time.h>
#include "./../headers/chrono.h"

#ifdef _WIN32

/**
 * @brief Platform-independent implementation of a timer returning time in seconds.
 * 
 * @return double The time in seconds.
 */
double second(void) {
    return (double) clock() / (double) CLOCKS_PER_SEC; // meglio di CLK_TCK
}

#else

#include <time.h>

#if defined(__MACH__) && defined(__APPLE__)
#include <mach/mach_time.h>
#endif

/**
 * @brief Retrieves the wall clock time in seconds.
 * 
 * @return double The wall clock time.
 */
double myWallTime(void) {



#ifdef __APPLE__
static double timeConvert = 0.0;
    if (timeConvert== 0.0)
    {
        mach_timebase_info_data_t timeBase;
        //@COMPLEX@: High-resolution timer implementation for macOS using mach_absolute_time and timebase conversion.
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

/**
 * @brief Platform-independent implementation of a timer returning time in seconds.
 * 
 * @return double The time in seconds.
 */
double second(void) {
    return myWallTime();
}

#endif
