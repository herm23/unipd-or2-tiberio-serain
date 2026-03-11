#ifndef UTILS_H
#define UTILS_H

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <cplex.h>


typedef struct {
    int nnodes;
    double *demand;
    double *xcoord;
    double *ycoord;
    int depot;
    double capacity;
    int nveh;

    // parameters
    int model_type;
    int old_benders;
    int randomseed;
    int num_threads;
    double timelimit; // overall time limit, in sec.s
    char input_file[1000]; // input file
    char node_file[1000]; // cplex node file
    int available_memory;
    int max_nodes; // max n. of branching nodes in the final run (-1 unlimited)
    double cutoff; // cutoff (upper bound) for master
    int integer_costs;

    //global data
    double tstart;
    double zbest; // best sol. available
    double tbest; // time for the best sol. available
    double *best_sol; // best sol. available
    double best_lb; // best lower bound available
    double *load_min; // minimum load when leaving a node
    double *load_max; // maximum load when leaving a node

    // model;
    //@COMPLEX@: Offsets for different sets of variables (x, u, etc.) in the CPLEX model's flattened variable array.
    int xstart;
    int qstart;
    int bigqstart;
    int sstart;
    int bigsstart;
    int ystart;
    int fstart;
    int zstart;
} instance;

//inline
/**
 * @brief Returns the maximum of two integers.
 * 
 * @param i1 First integer.
 * @param i2 Second integer.
 * @return int The maximum of the two integers.
 */
inline int imax(int i1, int i2) { return (i1 > i2) ? i1 : i2; }

/**
 * @brief Returns the minimum of two doubles.
 * 
 * @param d1 First double.
 * @param d2 Second double.
 * @return double The minimum of the two doubles.
 */
inline double dmin(double d1, double d2) { return (d1 < d2) ? d1 : d2; }

/**
 * @brief Returns the maximum of two doubles.
 * 
 * @param d1 First double.
 * @param d2 Second double.
 * @return double The maximum of the two doubles.
 */
inline double dmax(double d1, double d2) { return (d1 > d2) ? d1 : d2; }


/**
 * @brief Shuffles an array of unsigned integers randomly.
 * 
 * @param array The array to be shuffled.
 * @param N The number of elements in the array.
 * @return short Returns 0 on success, non-zero on error.
 */
short random_shuffle(unsigned int *array, unsigned int N);

/**
 * @brief Initializes a solution array with an identity mapping (0, 1, 2, ..., N-1).
 * 
 * @param N The number of nodes in the solution.
 * @return unsigned int* Pointer to the newly allocated and initialized solution array.
 */
unsigned int *initialize_solution(unsigned int N);

void swap(void *a, void *b, size_t size);

/**
 * @brief Parses the command line arguments and populates the instance structure.
 * 
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @param inst Pointer to the instance structure to be populated.
 */
void parse_command_line(int argc, char **argv, instance *inst);

/**
 * @brief Reads the input file and populates the instance structure.
 * 
 * @param inst Pointer to the instance structure to be populated.
 */
void read_input(instance *inst);

void print_error(const char *err);
void free_instance(instance *inst);
void print_array(int *array, int size);

#endif