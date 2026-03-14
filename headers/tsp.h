//
// Created by filip on 28/02/2026.
//

#ifndef UNIPD_OR2_TIBERIO_SERAIN_TSP_H
#define UNIPD_OR2_TIBERIO_SERAIN_TSP_H

#include "utils.h"
#include <math.h>
#include <string.h>

// #include <pthread.h>

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

short setDistance(unsigned int i, unsigned int j, const instance *inst, double dist);

double getDistance(unsigned int i, unsigned int j, const instance *inst);

//TODO: Remove method
/**
 * @brief Optimizes the TSP using CPLEX.
 * 
 * @param inst Pointer to the instance structure.
 * @param sol Pointer to the solution array to be updated with the optimized tour.
 */
void TSPopt(instance *inst, unsigned int *sol);

short tspNNBatch(instance *inst, unsigned int *sol);

//TODO: Manage errors
double tspNN(instance *inst, unsigned int startingNode, unsigned int *sol);


#endif //UNIPD_OR2_TIBERIO_SERAIN_TSP_H
