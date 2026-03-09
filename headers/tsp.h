//
// Created by filip on 28/02/2026.
//

#ifndef UNIPD_OR2_TIBERIO_SERAIN_TSP_H
#define UNIPD_OR2_TIBERIO_SERAIN_TSP_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <cplex.h>
// #include <pthread.h>

#define VERBOSE				    10		// printing level  (=10 only incumbent, =20 little output, =50-60 good, =70 verbose, >=100 cplex log)



//hard-wired parameters
#define XSMALL		  		  1e-5 		// 1e-4*	// tolerance used to decide ingerality of 0-1 var.s
#define EPSILON		  		  1e-9		// 1e-9		// very small numerical tolerance
#define TICKS_PER_SECOND 	  1000.0  	// cplex's ticks on Intel Core i7 quadcore @2.3GHZ


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

/**
 * @brief Plots the tour using an external tool (e.g., gnuplot).
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array representing the tour.
 * @param only_instance Flag to plot only the instance nodes without the tour.
 */
void plot_tour(const instance *inst, const unsigned int *solution, int only_instance);

/**
 * @brief Checks if a solution represents a valid tour (Hamiltonian cycle).
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 0 if the tour is valid, non-zero otherwise.
 */
short check_tour(const instance *inst, const unsigned int *solution);

/**
 * @brief Optimizes the TSP using CPLEX.
 * 
 * @param inst Pointer to the instance structure.
 * @param sol Pointer to the solution array to be updated with the optimized tour.
 */
void TSPopt(instance *inst, unsigned int *sol);


#endif //UNIPD_OR2_TIBERIO_SERAIN_TSP_H
