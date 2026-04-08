#ifndef CONSTS_H
#define CONSTS_H

/**
 * Verbosity levels:
 * - 0: SILENT    - No output except errors.
 * - 1: ESSENTIAL - Basic solving information (e.g., total time, best cost).
 * - 2: INFO      - Echoes command line arguments, parameters and instance summary.
 * - 3: DEBUG     - Solver progress and important intermediate results.
 * - 4: VERBOSE   - Detailed parser information and input reading.
 * - 5: TRACE     - Low-level tracing (raw input lines, tokenized parameters).
 */

#define SILENT          0
#define ESSENTIAL       1
#define INFO            2
#define DEBUG           3
#define VERBOSE_LEVEL   4
#define TRACE           5

#define DEFAULT_VERBOSITY INFO

// hard-wired parameters
#define XSMALL                1e-5      
#define EPSILON               1e-9      
#define TICKS_PER_SECOND      1000.0

#define DEFAULT_INPUT "NULL"
//random ranges constants
#define DEFAULTSEEDVAL        -1
#define XMAX                  10000.0
#define YMAX                  10000.0

#endif // CONSTS_H