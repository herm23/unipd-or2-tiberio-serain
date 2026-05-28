#ifndef UNIPD_OR2_TIBERIO_SERAIN_TSP_H
#define UNIPD_OR2_TIBERIO_SERAIN_TSP_H

#include "utils.h"

#include <cplex.h>


enum TSP_SOLVING_STRATEGY {
    // only NN, tries all starting nodes until time or nodes run out
    NN,

    // Executes an NN with a single starting node, executes a 2-opt until local optimum or end of time. At the end, if there are still nodes or time, restarts from the beginning with another starting node
    NN_TWO_OPT_MULTISTART,

    // NN tries all starting nodes until time (construction) or nodes run out, then at the end ONE pass of 2-opt until local optimum or end of refinement time.
    NN_AND_TWO_OPT,

    // NN tries all starting nodes until time (construction) or nodes run out, then at the end a loop of (2-opt -> NKick) until the end of time
    NN_AND_VNS,

    NN_AND_TABUSEARCH,

    // only GRASP, tries all starting nodes until time or nodes run out
    GRASP,

    // Executes a GRASP with a single starting node, executes a 2-opt until local optimum or end of time. At the end, if there are still nodes or time, restarts from the beginning with another starting node
    GRASP_TWO_OPT_MULTISTART,

    // NN tries all starting nodes until time (construction) or nodes run out, then at the end ONE pass of 2-opt until local optimum or end of refinement time.
    GRASP_AND_TWO_OPT,

    // GRASP tries all starting nodes until time (construction) or nodes run out, then at the end a loop of (2-opt -> NKick) until the end of time
    GRASP_AND_VNS,

    GRASP_AND_TABUSEARCH,

    EXTRA_MILEAGE,

    CPLEX
};

enum TSP_REFINEMENT_STRATEGY {
    NONE,
    TWO_OPT,
};

enum CPLEX_MODE {
    BENDERS_LOOP,
    CALLBACKS,
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

/**
 * @brief Plots the current solution (including subtours) using gnuplot by visualizing edges from the successor array.
 * 
 * @param inst Pointer to the instance structure containing coordinates and plot pipe.
 * @param succ Array where succ[i] represents the index of the node followed by node i.
 */
void plot_solution_edges(instance *inst, const int *succ);

void plot_iteration(instance *inst, double curr_cost, double curr_opt_cost, char *section);

//endregion Plots

/**
 * @brief Checks if a solution represents a valid tour (Hamiltonian cycle).
 *
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 0 if the tour is valid, non-zero otherwise.
 */
void update_instance_solution(instance *inst, unsigned int *new_sol, double new_cost, double new_time);

short is_solution_valid(const instance *inst, const unsigned int *solution, double expected_cost);

/**
 * @brief Calculates the total cost of a given solution.
 *
 * @param inst Pointer to the instance structure.
 * @param sol The solution array representing the solution.
 * @return double The total cost of the solution.
 */
double calculate_solution_cost(const instance *inst, const unsigned int *sol);

//region Distances

/**
 * @brief Sets the precalculated distance between two nodes in the distance matrix.
 *
 * @param inst Pointer to the instance structure.
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param dist The distance to be set.
 * @return short Returns 0 on success, non-zero otherwise.
 */
short set_precalculated_distance(const instance *inst, unsigned int i, unsigned int j, double dist);

/**
 * @brief Retrieves the precalculated distance between two nodes from the distance matrix.
 *
 * @param inst Pointer to the instance structure.
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @return double The precalculated distance between node i and node j.
 */
double get_precalculated_distance(const instance *inst, unsigned int i, unsigned int j);

//endregion Distances


//region Heuristics

//region NN
/**
 * @brief Executes the Nearest Neighbor (NN) heuristic to solve the TSP.
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the heuristic to run.
 * @param refinement The refinement strategy to be applied after the heuristic.
 * @return short Returns 0 on success, non-zero if the time limit was exceeded.
 */
short greedy_nn_heuristic(instance *inst, double time_limit, enum TSP_REFINEMENT_STRATEGY refinement);


/**
 * @brief Performs a single iteration of the Nearest Neighbor heuristic starting from a specific node.
 *
 * @param inst Pointer to the instance structure.
 * @param startingNode The index of the node to start the tour from. If -1, a random node is chosen.
 * @param GRASP_alpha The alpha parameter for the GRASP (Greedy Randomized Adaptive Search Procedure) heuristic. It controls the degree of randomization in the construction phase: 0 is purely greedy, 1 is purely random.
 * @param sol Pointer to the array where the resulting tour will be stored.
 * @return double The cost of the generated tour.
 */
double greedy_nn_heuristic_iteration(const instance *inst, unsigned int startingNode, double GRASP_alpha,
                                     unsigned int *sol);

//endregion NN


short extra_mileage_heuristic(instance *inst);


//endregion Heuristics


//region Refinements
/**
 * @brief Applies the 2-opt refinement strategy to an existing TSP solution.
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the refinement.
 * @param sol Pointer to the solution array to be refined.
 * @param zbest Pointer to the variable storing the cost of the best solution found.
 * @param tbest Pointer to the variable storing the time when the best solution was found.
 * @param enable_plot Flag to enable (1) or disable (0) Gnuplot visualizations.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short twoOpt_refinement(instance *inst, double time_limit, unsigned int *sol, double *zbest, double *tbest, short enable_plot);

/**
 * @brief Applies the 3-opt kick by swapping 2 parts of the solution array
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the refinement.
 * @param sol Pointer to the solution array to be refined.
 * @param zbest Pointer to the variable storing the cost of the best solution found.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short vns_kick(instance *inst, double time_limit, unsigned int *sol, double *zbest);

/**
 * @brief Performs a single iteration of the 2-opt refinement.
 *
 * @param inst Pointer to the instance structure.
 * @param sol Pointer to the solution array to be refined.
 * @return double The improvement (reduction in cost) achieved by the iteration.
 */
double twoOpt_refinement_iteration(const instance *inst, unsigned int *sol);

//endregion Refinements


int TSPopt(instance *inst, enum CPLEX_MODE mode);

int xpos(int i, int j, instance *inst);

void build_model(instance *inst, CPXENVptr env, CPXLPptr lp);

void build_sol(const double *xstar, instance *inst, int *succ, int *comp, int *ncomp);


#endif //UNIPD_OR2_TIBERIO_SERAIN_TSP_H
