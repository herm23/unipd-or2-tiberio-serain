// DO NOT IMPORT THIS!

#ifndef UTILS_H
#define UTILS_H
#include <stddef.h>
#include <stdio.h>


//region STRUCTURES
typedef struct {
    int nnodes;
    double *xcoord;
    double *ycoord;
    double *distances;

    // parameters
    int randomseed;
    int model_type;
    double timelimit; // overall time limit, in sec.s
    char input_file[1000]; // input file

    //global data
    double zbest; // best sol. available
    double tbest; // time for the best sol. available

    double zstart;
    double tstart;

    unsigned int *solution; // best sol. available

    FILE *plot;
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
void reverse_subarray_circular(unsigned int *C, unsigned int N, unsigned int l, unsigned int r);

/**
 * @brief Shuffles an array of unsigned integers randomly.
 * 
 * @param array The array to be shuffled.
 * @param N The number of elements in the array.
 * @return short Returns 0 on success, non-zero on error.
 */
short random_array_shuffle(unsigned int *array, unsigned int N);

/**
 * @brief Initializes a solution array with an identity mapping (0, 1, 2, ..., N-1).
 * 
 * @param N The number of nodes in the solution.
 * @return unsigned int* Pointer to the newly allocated and initialized solution array.
 */
unsigned int *initialize_identity_solution(unsigned int N);

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
double calculate_distance(unsigned int i, unsigned int j, const instance *inst);

/**
 * @brief Calculates the squared distance between two nodes.
 * 
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated squared distance between node i and node j.
 */
double calculate_squared_distance(unsigned int i, unsigned int j, const instance *inst);

//endregion UTILITY

#endif
