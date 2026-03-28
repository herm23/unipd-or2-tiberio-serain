//
// Created by filip on 28/02/2026.
//

#ifndef UNIPD_OR2_TIBERIO_SERAIN_TSP_H
#define UNIPD_OR2_TIBERIO_SERAIN_TSP_H

#include "utils.h"

enum TSP_SOLVING_STRATEGY {
    LOW
};

enum TSP_REFINEMENT_STRATEGY {
    NONE,
    TWO_OPT
};


//region Plots
/**
 * @brief Plots the solution using gnuplot
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array representing the tour.
 * @param only_instance Flag to plot only the instance nodes without the tour.
 */
void plot_solution(instance *inst, const unsigned int *solution, int only_instance, short pause);


//endregion Plots

/**
 * @brief Checks if a solution represents a valid tour (Hamiltonian cycle).
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 0 if the tour is valid, non-zero otherwise.
 */
short is_solution_valid(const instance *inst, const unsigned int *solution);

/**
 * @brief Calculates the total cost of a given solution.
 *
 * @param inst Pointer to the instance structure.
 * @param sol The solution array representing the solution.
 * @return double The total cost of the solution.
 */
double calculate_solution_cost(const instance *inst, const unsigned int *sol);

//region Distances

short set_precalculated_distance(const instance *inst, unsigned int i, unsigned int j, double dist);

double get_precalculated_distance(const instance *inst, unsigned int i, unsigned int j);

//endregion Distances


//region Heuristics

//region NN
short greedy_nn_heuristic(instance *inst, double time_limit, enum TSP_REFINEMENT_STRATEGY refinement);

double greedy_nn_heuristic_iteration(const instance *inst, unsigned int startingNode, unsigned int *sol);

//endregion NN

//endregion Heuristics


//region Refinements
short twoOpt_refinement(instance *inst, double time_limit, unsigned int *sol, double *zbest, double *tbest);

double twoOpt_refinement_iteration(const instance *inst, unsigned int *sol);

//endregion Refinements


#endif //UNIPD_OR2_TIBERIO_SERAIN_TSP_H
