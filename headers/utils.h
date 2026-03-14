#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cplex.h>

//region STRUCTURES
//TODO: Check used variables
typedef struct {
    int nnodes;
    double *demand;
    double *xcoord;
    double *ycoord;
    double *distances;
    int depot;
    double capacity;
    int nveh;

    // parameters
    //NOTE: The fields randomseed and num_nodes must be used togheter
    int randomseed;
    int model_type;
    int old_benders;
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
//endregion STRUCTURES

//region MATH UTILITIES
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
//endregion MATH UTILITIES

//region UTILITY
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

/**
 * @brief Generic swap function for two elements of a given size.
 * 
 * @param a Pointer to the first element.
 * @param b Pointer to the second element.
 * @param size Size of the elements to be swapped.
 */
void swap(void *a, void *b, size_t size);

/**
 * @brief Implementation of the command line parser.
 * 
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @param inst Pointer to the instance structure to be populated.
 */
void parse_command_line(int argc, char **argv, instance *inst);

/**
 * @brief Implementation of the input reader.
 * 
 * @param inst Pointer to the instance structure to be populated.
 */
void read_input(instance *inst);

/**
 * @brief Prints an error message and terminates the program.
 * 
 * @param err The error message to be printed.
 */
void print_error(const char *err);

/**
 * @brief Frees the memory allocated for the instance.
 * 
 * @param inst Pointer to the instance structure to be freed.
 */
void free_instance(instance *inst);

/**
 * @brief Prints the elements of an integer array.
 * 
 * @param array The array to be printed.
 * @param size The number of elements in the array.
 */
void print_array(int *array, int size);

/**
 * @brief Initializes the distance matrix for the instance.
 * 
 * @param inst Pointer to the instance structure.
 */
void init_distances(instance *inst);

/**
 * @brief Calculates the distance between two nodes based on the instance's edge weight type.
 *
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated distance between node i and node j.
 */
double dist(unsigned int i, unsigned int j, const instance *inst);

/**
 * @brief Calculates the squared distance between two nodes.
 * 
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated squared distance between node i and node j.
 */
double distSquared(unsigned int i, unsigned int j, const instance *inst);
//endregion UTILITY

#endif
