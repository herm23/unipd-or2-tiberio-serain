#ifndef CONSTS_H
#define CONSTS_H

/**
 * Verbosity levels (VERBOSE):
 * - >= 1: Basic solving information (e.g., total time).
 * - >= 2: Echoes command line arguments.
 * - >= 10: Prints current parameters and instance details.
 * - >= 100: Displays parser startup information.
 * - >= 1000: Shows detailed input reading and final solution tour.
 * - >= 2000: Prints raw input lines during parsing.
 * - >= 3000: Displays tokenized parameter names during parsing.
 */

#define VERBOSE  10 

// hard-wired parameters
#define XSMALL                1e-5      
#define EPSILON               1e-9      
#define TICKS_PER_SECOND      1000.0    

#endif // CONSTANTS_H