#include "./../headers/tsp.h"
#include "./../headers/utils.h"

#include "../headers/chrono.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

// #ifdef _WIN32
// #include <windows.h>
// #else
// #include <unistd.h>
// #endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "concorde.h"

//TODO: Move the following regions into the utils.c fi

//region Time budget (T4 fix: single source of truth for the residual time)

#define TIME_EPS 0.1 // stop handing CPLEX time when fewer than this many seconds remain

// Remaining wall-clock budget (seconds) before the global time limit; clamped to >= 0.
static double time_left(const instance *inst) {
    double r = inst->timelimit - (second() - inst->tstart);
    return r > 0.0 ? r : 0.0;
}

//endregion

//region Update Matheuristics-prob

static void update_fixing_probability(double *p, int iteration, instance *inst) {
    // NOTE: Update every 10 iterations with a uniform distribution between 0.2 and 0.5
    if (iteration > 0 && iteration % 10 == 0) {
        double min_p = 0.2;
        double max_p = 0.5;
        *p = min_p + ((double)rand() / RAND_MAX) * (max_p - min_p);

        if (inst->verbosity >= INFO) {
            printf("[Hard-Fixing] Iteration %d: updating probability p to %.3lf\n", iteration, *p);
        }
    }
}

//endregion

//region Data-structure convertions

void succ_to_permutation(int nnodes, const int *succ, unsigned int *sol_perm) {
    // C3 fix: detect a non-Hamiltonian `succ` (subtours / invalid links) instead of silently
    // producing a corrupted permutation. A single tour visits each node exactly once.
    unsigned char *seen = calloc(nnodes, sizeof(unsigned char));
    int currNode = 0;
    for (int i = 0; i < nnodes; i++) {
        int next = succ[currNode];
        if (next < 0 || next >= nnodes || seen[next]) {
            free(seen);
            print_error_and_die("succ_to_permutation: succ is not a single Hamiltonian tour (subtour/invalid succ)");
        }
        seen[next] = 1;
        sol_perm[i] = (unsigned int) next;
        currNode = next;
    }
    free(seen);
}

void permutation_to_succ(int nnodes, const unsigned int *sol_perm, int *succ) {
    for (int i = 0; i < nnodes; i++) {
        unsigned int u = sol_perm[i];
        unsigned int v = sol_perm[(i + 1) % nnodes];
        succ[u] = (int) v;
    }
}

//endregion

//region Plots
/**
 * @brief Plots the solution using gnuplot.
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array representing the tour.
 * @param only_instance Flag to plot only the instance nodes without the tour.
 * @param pause Flag to pause after plotting.
 */
void plot_solution(instance *inst, const unsigned int *solution, int only_instance, short pause) {
    if (inst->no_plot) return;
    /* ----------- Basic error checking ----------- */
    if (inst->verbosity >= SILENT) {
        if (inst == NULL) {
            fprintf(stderr, "plot_tour error: inst is NULL\n");
            return;
        }

        if (inst->nnodes <= 0) {
            fprintf(stderr, "plot_tour error: invalid number of nodes\n");
            return;
        }

        if (inst->xcoord == NULL || inst->ycoord == NULL) {
            fprintf(stderr, "plot_tour error: coordinate arrays are NULL\n");
            return;
        }

        if (!only_instance && solution == NULL) {
            fprintf(stderr, "plot_tour error: solution is NULL\n");
            return;
        }
    }

    /* ----------- Open gnuplot pipe ----------- */
    //@COMPLEX@: Opens a persistent pipe to gnuplot for visualization. This requires gnuplot to be installed and in the system PATH.

    if (inst->plot == NULL) {
        inst->plot = popen("gnuplot -persist", "w");
        if (inst->plot == NULL) {
            perror("plot_tour error: popen failed");
            return;
        }
        // fprintf(inst->plot, "set term qt\n");

        //TODO: With this the log "QSocketNotifier: Can only be used with threads started with QThread" disappears
        //Lo teniamo Filippo ?
        // fprintf(inst->plot, "set term wxt\n"); 
    }

    /* ----------- Gnuplot configuration ----------- */
    // fprintf(inst->plot, "clear\n");
    fprintf(inst->plot, "set title 'TSP Visualization'\n");
    fprintf(inst->plot, "set xlabel 'X'\n");
    fprintf(inst->plot, "set ylabel 'Y'\n");
    fprintf(inst->plot, "set grid\n");
    fprintf(inst->plot, "set key off\n");
    fprintf(inst->plot, "set size ratio -1\n"); /* keep aspect ratio */

    if (only_instance) {
        /* ---- Plot only nodes ---- */
        fprintf(inst->plot, "plot '-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(inst->plot, "%f %f\n", inst->xcoord[i], inst->ycoord[i]);
        }

        fprintf(inst->plot, "e\n");
    } else {
        /* ---- Plot nodes + tour with ARROWS ---- */
        // "with vectors" disegna una freccia da (x,y) a (x+dx, y+dy)
        fprintf(inst->plot,
                "plot '-' with vectors head filled lw 2 lc rgb 'red', "
                "'-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        /* ---- Tour edges as vectors ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            int curr = solution[i];
            int next = solution[(i + 1) % inst->nnodes]; // Il prossimo nodo (chiusura automatica)

            double x = inst->xcoord[curr];
            double y = inst->ycoord[curr];
            // Calcoliamo lo spostamento (delta) verso il prossimo nodo
            double dx = inst->xcoord[next] - x;
            double dy = inst->ycoord[next] - y;

            fprintf(inst->plot, "%f %f %f %f\n", x, y, dx, dy);
        }
        fprintf(inst->plot, "e\n");

        /* ---- Nodes (stessa logica di prima) ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(inst->plot, "%f %f\n", inst->xcoord[i], inst->ycoord[i]);
        }
        fprintf(inst->plot, "e\n");
    }


    fflush(inst->plot);

    if (pause && inst->plot != NULL) {
        if (inst->verbosity >= ESSENTIAL) printf("\nPress ENTER to close the graph and resume the program...");
        getchar();
    }
}

/**
 * @brief Plots the current solution (including subtours) using gnuplot by visualizing edges from the successor array.
 * 
 * @param inst Pointer to the instance structure containing coordinates and plot pipe.
 * @param succ Array where succ[i] represents the index of the node followed by node i.
 */
void plot_solution_edges(instance *inst, const int *succ) {
    if (inst->no_plot || inst->plot == NULL) return;

    /* ---- Gnuplot configuration ---- */
    fprintf(inst->plot, "set title 'TSP Solution (including subtours)'\n");
    fprintf(inst->plot, "set xlabel 'X'\n");
    fprintf(inst->plot, "set ylabel 'Y'\n");
    fprintf(inst->plot, "set grid\n");
    fprintf(inst->plot, "set key off\n");
    fprintf(inst->plot, "set size ratio -1\n"); /* Maintain aspect ratio */

    /* ---- Plot edges and nodes ---- */
    // 'with vectors' draws from (x,y) to (x+dx, y+dy). 
    // 'nohead' can be used if you don't want arrows, but 'head filled' is better for direction.
    fprintf(inst->plot,
            "plot '-' with vectors head filled lw 2 lc rgb 'red', "
            "'-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

    /* ---- 1. Draw edges based on the successor array ---- */
    for (int i = 0; i < inst->nnodes; ++i) {
        int next = succ[i];
        
        // Skip if successor is invalid or points to itself
        if (next < 0 || next >= inst->nnodes || next == i) continue;

        double x = inst->xcoord[i];
        double y = inst->ycoord[i];
        
        // IMPORTANT: dx and dy must be the RELATIVE distance
        double dx = inst->xcoord[next] - x;
        double dy = inst->ycoord[next] - y;

        fprintf(inst->plot, "%f %f %f %f\n", x, y, dx, dy);
    }
    fprintf(inst->plot, "e\n"); // End of vector data

    /* ---- 2. Draw nodes ---- */
    for (int i = 0; i < inst->nnodes; ++i) {
        fprintf(inst->plot, "%f %f\n", inst->xcoord[i], inst->ycoord[i]);
    }
    fprintf(inst->plot, "e\n"); // End of point data

    fflush(inst->plot);
}

void plot_iteration(instance *inst, double curr_cost, double curr_opt_cost, char *section) {
    return;
    if (inst->cost_plot == NULL) {
        inst->cost_plot = fopen("cost.csv", "w");
    }

    fprintf(inst->cost_plot, "%lf,%lf,%s\n", curr_cost, curr_opt_cost, section);
    fflush(inst->cost_plot);
}

//endregion Plots

/**
 * @brief Checks if a solution represents a valid tour (Hamiltonian cycle).
 *
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 1 if the tour is valid, 0 otherwise.
 */
short is_solution_valid(const instance *inst, const unsigned int *solution, const double expected_cost) {
    //Return 0 if valid, -1 otherways
    if (inst == NULL || solution == NULL) return -1;

    // C4 fix: compare with tolerance instead of exact double equality (the caller may maintain
    // the cost incrementally, so rounding order can differ from a fresh recomputation).
    const double actual_cost = calculate_solution_cost(inst, solution);
    if (fabs(expected_cost - actual_cost) > EPSILON) {
        printf("ERROR: Expected cost %lf, calculated cost %lf\n", expected_cost, actual_cost);
        return 0;
    }

    //NOTE: NO CHECKS ON INSTANCE OR SOLUTION VALIDITY (implicits on previous functions)

    const unsigned int N = inst->nnodes;
    unsigned char *visited = calloc(N, sizeof(unsigned char)); //with calloc we avoid the for loop for zero-init

    if (!visited) return 0;

    for (unsigned int i = 0; i < N; ++i) {
        unsigned int node = solution[i];

        //@COMPLEX@: Validates a Hamiltonian cycle by ensuring each node is visited exactly once using a bit/byte-set.
        if (node >= N || visited[node]) {
            //we can do only a single pass like this  if we know the range of elements
            free(visited);
            return 0;
        }

        visited[node] = 1;
    }

    free(visited);
    return 1;
}

short log_and_check_sol(const instance *inst, const unsigned int *solution, const double expected_cost) {
    const short valid = is_solution_valid(inst, solution, expected_cost);
    if (valid) {
        if (inst->verbosity >= DEBUG) {
            printf("[VALIDATION] Success: Tour is a valid Hamiltonian cycle. Cost: %lf\n", expected_cost);
            // plot_solution(inst, solution, 0, 0);
        }
    } else {
        // Questo è il print più importante in fase di sviluppo
        printf("[VALIDATION] ERROR: Invalid tour detected! (Check for duplicates or missing nodes)\n");
    }

    return valid;
}

/**
 * @brief Calculates the total cost of a given solution.
 *
 * @param inst Pointer to the instance structure.
 * @param sol The solution array representing the solution.
 * @return double The total cost of the solution.
 */
double calculate_solution_cost(const instance *inst, const unsigned int *sol) {
    double cost = 0.0;
    int N = inst->nnodes;

    for (int i = 0; i < N - 1; i++)
        cost += get_precalculated_distance(inst, sol[i], sol[i + 1]);

    cost += get_precalculated_distance(inst, sol[N - 1], sol[0]);
    return cost;
}


//region Distances

/**
 * @brief Calculates the index for an upper triangular distance matrix (without diagonal).
 *
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param N Total number of nodes.
 * @return unsigned int The index in the 1D distance array.
 */
unsigned int index_upper(unsigned int i, unsigned int j, unsigned int N) {
    return i * N - i * (i + 1) / 2 + (j - i - 1);
}

/**
 * @brief Sets the precalculated distance between two nodes in the distance matrix.
 *
 * @param inst Pointer to the instance structure.
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param dist The distance to be set.
 * @return short Returns 0 on success, 1 if inst is NULL, 2 if i == j.
 */
short set_precalculated_distance(const instance *inst, unsigned int i, unsigned int j, const double dist) {
    if (inst == NULL) return 1;

    if (i == j)
        return 2;

    if (i > j) {
        const unsigned int tmp = i;
        i = j;
        j = tmp;
    }

    inst->distances[index_upper(i, j, inst->nnodes)] = dist;
    return 0;
}

/**
 * @brief Retrieves the precalculated distance between two nodes from the distance matrix.
 *
 * @param inst Pointer to the instance structure.
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @return double The precalculated distance between node i and node j, -1 if inst is NULL, 2 if i == j.
 */
double get_precalculated_distance(const instance *inst, unsigned int i, unsigned int j) {
    if (inst == NULL) return -1;

    if (i == j)
        return 2;

    if (i > j) {
        const unsigned int tmp = i;
        i = j;
        j = tmp;
    }

    return inst->distances[index_upper(i, j, inst->nnodes)];
}

//endregion Distances


/**
 * @brief Executes the Nearest Neighbor (NN) heuristic to solve the TSP.
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the heuristic to run.

 * @param refinement The refinement strategy to be applied after each iteration.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short greedy_nn_heuristic(instance *inst, double time_limit, const enum TSP_REFINEMENT_STRATEGY refinement) {
    if (inst->verbosity >= DEBUG) printf("\t## START greedy_nn_heuristic\n");

    short exitStatus = 0;

    unsigned int *nodes_order = initialize_identity_solution(inst->nnodes);
    random_array_shuffle(nodes_order, inst->nnodes);

    for (int i = 0; i < inst->nnodes; i++) {
        unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
        double temp_solution_cost = greedy_nn_heuristic_iteration(inst, nodes_order[i], inst->GRASP_alpha,
                                                                  temp_solution);

        double curr_time = second() - inst->tstart;
        if (inst->verbosity >= DEBUG) {
            printf("\t -> S-node %7d,  cost %10.2lf,  cost %10.2lf, time %8.2lf", nodes_order[i], temp_solution_cost,
                   calculate_solution_cost(inst, temp_solution),
                   curr_time);
        }

        plot_iteration(inst, temp_solution_cost, inst->zbest, "nn");

        switch (refinement) {
            case TWO_OPT: {
                //NOTE: Performing vns kicks
                //2-opt
                if (inst->verbosity >= DEBUG) printf("\n");
                short res = twoOpt_refinement(inst, time_limit, temp_solution, &temp_solution_cost, &curr_time, 1);
                if (res == -1) {
                    exitStatus = -1;
                    break;
                }
                //  if (inst->verbosity >= DEBUG) {
                printf("\t -> AFTER twoOpt_refinement cost %10.2lf, time %8.2lf",
                       temp_solution_cost,
                       curr_time);
                // }
            }
            break;

            case NONE: {
            }
            break; //Nothing to do here
        }

        if (inst->zbest < 0 || temp_solution_cost < inst->zbest - EPSILON) {
            if (is_solution_valid(inst, temp_solution, temp_solution_cost)) {
                inst->zbest = temp_solution_cost;
                inst->tbest = curr_time;
                memcpy(inst->solution, temp_solution, sizeof(unsigned int) * inst->nnodes);
                if (inst->verbosity >= INFO) {
                    printf("\n[NN] - New incumbent: cost = %.2lf, time = %.2lf\n", inst->zbest, inst->tbest);
                }
            } else {
                if (inst->verbosity >= SILENT)
                    printf(
                        "\n ------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------");
            }
        }

        if (inst->verbosity >= DEBUG) printf("\n\n");
        free(temp_solution);

        if (curr_time > time_limit) {
            exitStatus = 1;
            break;
        }
        plot_solution(inst, inst->solution, 0, 0);
        // #ifdef _WIN32
        //         Sleep(100);
        // #else
        //         sleep(0.1);
        // #endif
    }

    free(nodes_order);
    return exitStatus;
}

/**
 * @brief Executes the Extra Mileage heuristic to solve the TSP.
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the heuristic to run.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short extra_mileage_heuristic(instance *inst) {
    if (inst->verbosity >= DEBUG) printf("\t## START extra_mileage_heuristic\n");

    const int N = inst->nnodes;
    unsigned int *sol = malloc(N * sizeof(unsigned int));
    unsigned char *visited = calloc(N, sizeof(unsigned char));

    // 1. Initial sub-tour: start with the two most distant nodes
    unsigned int node1 = 0, node2 = 1;
    double max_distance = get_precalculated_distance(inst, 0, 1);

    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            double dist = get_precalculated_distance(inst, i, j);
            if (dist > max_distance) {
                max_distance = dist;
                node1 = i;
                node2 = j;
            }
        }
    }

    sol[0] = node1;
    sol[1] = node2;
    visited[node1] = 1;
    visited[node2] = 1;

    int current_nodes_in_tour = 2;

    while (current_nodes_in_tour < N) {
        double min_extra_mileage = -1.0;
        unsigned int best_node = -1;
        int best_pos = -1;

        for (int k = 0; k < N; k++) {
            if (visited[k]) continue;

            for (int i = 0; i < current_nodes_in_tour; i++) {
                int j = (i + 1) % current_nodes_in_tour;

                double d_ik = get_precalculated_distance(inst, sol[i], k);
                double d_kj = get_precalculated_distance(inst, k, sol[j]);
                double d_ij = get_precalculated_distance(inst, sol[i], sol[j]);

                double delta = d_ik + d_kj - d_ij;

                if (min_extra_mileage < 0 || delta < min_extra_mileage - EPSILON) {
                    min_extra_mileage = delta;
                    best_node = k;
                    best_pos = i + 1;
                }
            }
        }

        for (int i = current_nodes_in_tour; i > best_pos; i--) {
            sol[i] = sol[i - 1];
        }
        sol[best_pos] = best_node;
        visited[best_node] = 1;
        current_nodes_in_tour++;
    }

    double final_cost = calculate_solution_cost(inst, sol);
    double final_time = second() - inst->tstart;
    update_instance_solution(inst, sol, final_cost, final_time);

    free(sol);
    free(visited);
    return 0;
}

/**
 * @brief Performs a single iteration of the Nearest Neighbor heuristic starting from a specific node.
 *
 * @param inst Pointer to the instance structure.
 * @param startingNode The index of the node to start the tour from. If -1, a random node is chosen.
 * @param GRASP_alpha The alpha parameter for the GRASP heuristic (0 = greedy, 1 = random).
 * @param sol Pointer to the array where the resulting tour will be stored.
 * @return double The cost of the generated tour.
 */
double greedy_nn_heuristic_iteration(const instance *inst, const unsigned int startingNode, double GRASP_alpha,
                                     unsigned int *sol) {
    const int nodes = inst->nnodes;

    //NOTE: Init starting random node
    unsigned int node_index = startingNode;
    if (startingNode == -1)
        node_index = rand() % nodes;

    swap(&sol[0], &sol[node_index], sizeof(int));

    double minDist = -1.0;
    unsigned int pointIndex = -1;

    double currSol = 0;
    double realDist = 0;

    //NOTE: The last 2 are already positioned
    for (unsigned int i = 0; i < nodes - 2; i++) {
        for (unsigned int j = i + 1; j < nodes; j++) {
            double tmpDist = get_precalculated_distance(inst, sol[i], sol[j]); // memoized
            double lrealDist = tmpDist;

            //GRASP HERE!
            tmpDist *= 1 + GRASP_alpha * rand_0_1();

            if (minDist < 0 || tmpDist < minDist - EPSILON) {
                minDist = tmpDist;
                realDist = lrealDist;
                pointIndex = j;
            }
        }
        currSol += realDist;
        swap(&sol[i + 1], &sol[pointIndex], sizeof(int));
        minDist = -1.0;
        pointIndex = -1;
    }

    currSol += get_precalculated_distance(inst, sol[nodes - 2], sol[nodes - 1]);
    currSol += get_precalculated_distance(inst, sol[nodes - 1], sol[0]);
    return currSol;
}


/**
 * @brief Applies the 2-opt refinement strategy to an existing TSP solution.
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the refinement.
 * @param sol Pointer to the solution array to be refined.
 * @param zbest Pointer to the variable storing the cost of the best solution found.
 * @param tbest Pointer to the variable storing the time when the best solution was found.
 * @param enable_plot Flag to enable (1) or disable (0) Gnuplot visualizations.
 * @return short Returns 0 on success, 1 if the time limit was exceeded, 2 if every candidate
 *         move became tabu and the search stopped early (Tabu Search mode only).
 */
short twoOpt_refinement(instance *inst, double time_limit, unsigned int *sol, double *zbest, double *tbest, short enable_plot) {
    if (inst->verbosity >= DEBUG) printf("\t\t## STARTING twoOpt_refinement\n");

    short exitStatus = 0;

    double initial_cost = *zbest;
    double curr_time = second() - inst->tstart;

    if (inst->verbosity >= DEBUG)
        printf("\t\t -> 2-opt INIT with cost  %20lf at time %20lf\n", initial_cost, curr_time);

    unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
    memcpy(temp_solution, sol, sizeof(unsigned int) * inst->nnodes);

    // NOTE: Conditional plotting based on the new parameter
    if (enable_plot) {
        plot_iteration(inst, initial_cost, *zbest < inst->zbest ? *zbest : inst->zbest, "2opt");
    }

    while (1) {
        // NOTE: Print the starting cost (it's redundant for debug)
        if (inst->verbosity >= DEBUG)
            printf("\t\t -> 2-opt INIT with cost    %20lf at time %20lf\n", initial_cost, curr_time);

        // NOTE: Tabu search is already implemented in this step due to parameter definition
        double temp_solution_cost_delta = 0;
        short iter_status = twoOpt_refinement_iteration(inst, temp_solution, &temp_solution_cost_delta);
        inst->iterations++; // NOTE: Update iterations after 2-opt !

        if (iter_status == 1) {
            // NOTE: Every candidate move is currently tabu: stop the search gracefully and
            // let the program continue its normal course (final cost/solution print, output file).
            if (inst->verbosity >= ESSENTIAL)
                printf("\n[Tabu] Stopping: every candidate move is currently tabu (tenure=%d) - "
                       "returning best solution found so far.\n", inst->tenure);
            exitStatus = 2;
            free(temp_solution);
            break;
        }

        initial_cost += temp_solution_cost_delta;
        curr_time = second() - inst->tstart;

        if (inst->verbosity >= DEBUG)
            printf("\t\t -> STEP made with new cost %20lf at time %20lf\n", initial_cost, curr_time);

        if (initial_cost < *zbest - EPSILON) {
            if (is_solution_valid(inst, temp_solution, initial_cost)) {
                *zbest = initial_cost;
                *tbest = curr_time;
                memcpy(sol, temp_solution, sizeof(unsigned int) * inst->nnodes);
                if (inst->verbosity >= INFO) {
                    printf("\n[2opt] - New local incumbent: cost = %.2lf, time = %.2lf\n", initial_cost, curr_time);
                }
            } else {
                // C10 fix: always free and bail on an invalid solution. The previous
                // `if (verbosity >= SILENT)` guard was always true, leaving a dead fall-through
                // that would otherwise keep an invalid tour.
                printf("------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------\n");
                free(temp_solution); // free before error exit to avoid leaks
                return -1;
            }
        }

        if (enable_plot) {
            plot_iteration(inst, initial_cost, *zbest < inst->zbest ? *zbest : inst->zbest, "2opt");
        }

        // NOTE: If i'm in a local minima and NOT in tabu
        if (temp_solution_cost_delta >= -EPSILON && inst->tenure == 0) {
            if (inst->VNS_kick_counter == 0) {
                free(temp_solution);
                break;
            }
            // Vns kick
            for (int i = 0; i < inst->VNS_kick_counter; i++) {
                // NOTE: If the time is finished we break the cycle and the all refinement prob.
                // T1 fix: pass this refinement's own budget, not the global inst->timelimit.
                int kick_status = vns_kick(inst, time_limit, temp_solution, &initial_cost);

                if (enable_plot) {
                    plot_iteration(inst, initial_cost, *zbest, "kick");
                }

                if (kick_status == 1)
                    break;

                if (inst->verbosity >= DEBUG) {
                    printf("\t -> AFTER VNS Kick %10.2lf, time %8.4lf \n", initial_cost, curr_time);
                }
            }
        }

        // NOTE: Check the time limit
        if (time_limit != 0 && curr_time > time_limit) {
            exitStatus = 1;
            free(temp_solution);
            break;
        }

        if (enable_plot) {
            plot_solution(inst, temp_solution, 0, 0);
        }
    }
    return exitStatus;
}


/**
 * @brief Performs a single iteration of the 2-opt refinement (best improvement).
 *
 * @param inst Pointer to the instance structure.
 * @param sol Pointer to the solution array to be refined.
 * @param deltaCost Output: the improvement (reduction in cost) achieved by the iteration.
 * @return short Returns 0 on success, 1 if every candidate move is currently tabu (no move applied).
 */
short twoOpt_refinement_iteration(const instance *inst, unsigned int *sol, double *deltaCost) {
    const int nodes = inst->nnodes;

    bool isDeltaSetted = false;
    double minDeltaCost = 0;
    unsigned int a = -1, b = -1;

    for (unsigned int i = 0; i < nodes; i++) {
        for (unsigned int j = i + 1; j < nodes; j++) {
            unsigned int ip1 = (i + 1) % nodes;
            unsigned int jp1 = (j + 1) % nodes;

            // evita di spezzare archi adiacenti
            if (i == j) continue;
            if (ip1 == j) continue; // archi consecutivi
            if (jp1 == i) continue; // caso circolare

            double aaprime = get_precalculated_distance(inst, sol[i], sol[ip1]);
            double bbprime = get_precalculated_distance(inst, sol[j], sol[jp1]);
            double ab = get_precalculated_distance(inst, sol[i], sol[j]);
            double aprimebprime = get_precalculated_distance(inst, sol[ip1], sol[jp1]);

            double deltaCost = (ab + aprimebprime) - (aaprime + bbprime);

            bool isTabu = (inst->tenure > 0)
                              ? ((inst->tabulist[i] != 0 && inst->iterations - inst->tabulist[i] <= inst->tenure) ||
                                 (inst->tabulist[j] != 0 && inst->iterations - inst->tabulist[j] <= inst->tenure))
                              : false;

            if (!isTabu && (!isDeltaSetted || deltaCost < (minDeltaCost - EPSILON))) {
                minDeltaCost = deltaCost;
                a = i;
                b = j;
                isDeltaSetted = true;
            }
        }
    }

    // NOTE: If every candidate (i,j) is currently tabu, a/b were never assigned: bail out
    // cleanly instead of falling through to the swap below (which would use a=b=-1).
    if (inst->tenure > 0 && !isDeltaSetted) {
        *deltaCost = 0;
        return 1;
    }

    //NOTE: If we are in the Tabu search mode i have to update the solution with the bost possible move founded
    if (minDeltaCost < -EPSILON || inst->tenure > 0) {
        //MOVE
        swap(&sol[a + 1], &sol[b], sizeof(int));
        reverse_subarray_circular(sol, nodes, a + 2, b);

        //Update tabu List
        if (inst->tenure > 0) {
            inst->tabulist[a] = inst->iterations;
            inst->tabulist[b] = inst->iterations;
        }
    }

    *deltaCost = minDeltaCost;
    return 0;
}


/**
 * @brief Applies the double bridge (4-opt) kick by reassembling the tour's 4 segments in a new order
 *
 * @param inst Pointer to the instance structure.
 * @param time_limit The maximum time allowed for the kick.
 * @param sol Pointer to the solution array to be kicked.
 * @param sol_cost Pointer to the variable storing the cost of the update solution.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short vns_kick(instance *inst, double time_limit, unsigned int *sol, double *sol_cost) {
    short exitStatus = 0;

    //NOTE: Selecting 3 different point cuts
    int cuts[3];
    cuts[0] = rand() % inst->nnodes;

    //NOTE: Avoiding of having equal cuts
    do {
        cuts[1] = rand() % inst->nnodes;
    } while (cuts[1] == cuts[0]);

    do {
        cuts[2] = rand() % inst->nnodes;
    } while (cuts[2] == cuts[0] || cuts[2] == cuts[1]);

    //Bubble sort
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (cuts[i] > cuts[j]) {
                int tmp = cuts[i];
                cuts[i] = cuts[j];
                cuts[j] = tmp;
            }
        }
    }

    int i = cuts[0];
    int j = cuts[1];
    int k = cuts[2];

    /* Original tour divided in:
       A: [0 ... i]
       B: [i+1 ... j]
       C: [j+1 ... k]
       D: [k+1 ... n-1]
       
       Una classica 4-opt kick (Double Bridge) riassembla come: A -> D -> C -> B
    */

    unsigned int *temp_solution = (unsigned int *) malloc(inst->nnodes * sizeof(unsigned int));
    int pos = 0;

    // Segment A
    for (int idx = 0; idx <= i; idx++) temp_solution[pos++] = sol[idx];
    // Segment D
    for (int idx = k + 1; idx < inst->nnodes; idx++) temp_solution[pos++] = sol[idx];
    // Segment C
    for (int idx = j + 1; idx <= k; idx++) temp_solution[pos++] = sol[idx];
    // Segment B
    for (int idx = i + 1; idx <= j; idx++) temp_solution[pos++] = sol[idx];

    //WARNING: This operation can be optimized
    *sol_cost = calculate_solution_cost(inst, temp_solution);

    memcpy(sol, temp_solution, inst->nnodes * sizeof(unsigned int));
    free(temp_solution);

    double curr_time = second() - inst->tstart;
    if (time_limit > 0 && curr_time > time_limit) {
        exitStatus = 1;
    }

    return exitStatus;
}


void apply_patching(instance *inst, int *succ, int *comp, int *ncomp, double *obj) {
    if (*ncomp <= 1) return;

    if (inst->verbosity >= INFO) {
        printf("[Patching] Starting global patching to merge %d components...\n", *ncomp);
    }

    while (*ncomp > 1) {
        double best_delta = 1e30;
        int best_i = -1;
        int best_j = -1;

        for (int i = 0; i < inst->nnodes; i++) {
            int next_i = succ[i];
            double dist_i_nexti = get_precalculated_distance(inst, i, next_i);

            for (int j = i + 1; j < inst->nnodes; j++) {
                if (comp[i] == comp[j]) continue;

                int next_j = succ[j];
                double dist_j_nextj = get_precalculated_distance(inst, j, next_j);

                double delta = (get_precalculated_distance(inst, i, next_j) + get_precalculated_distance(inst, j, next_i)) - (dist_i_nexti + dist_j_nextj);

                if (delta < best_delta) {
                    best_delta = delta;
                    best_i = i;
                    best_j = j;
                }
            }
        }

        if (best_i != -1 && best_j != -1) {
            int old_next_i = succ[best_i];
            int old_next_j = succ[best_j];
            succ[best_i] = old_next_j;
            succ[best_j] = old_next_i;

            // NOTE: Update the objective function value directly with the best delta found
            *obj += best_delta;

            int comp_to_replace = comp[best_j];
            int new_comp_label = comp[best_i];
            for (int k = 0; k < inst->nnodes; k++) {
                if (comp[k] == comp_to_replace) comp[k] = new_comp_label;
            }
            (*ncomp)--;
        } else {
            print_error_and_die("[Patching] No more components found!\n");
            break; // NOTE: Safety guard against infinite loops
        }
    }

    if (inst->verbosity >= INFO) {
        printf("[Patching] Finished patching heuristic\n");
    }
}

static void TSPopt_benders(instance *inst, CPXENVptr env, CPXLPptr lp, double *xstar,
                           int *succ, int *comp, int *ncomp, int *index, double *value, char **cname) {
    int error = 0;
    int secCount = 0;
    int izero = 0;

    do {
        double elapsed = second() - inst->tstart;
        double residual_time = time_left(inst); // T4 fix: shared residual helper + TIME_EPS guard

        if (residual_time <= TIME_EPS) break;

        CPXsetdblparam(env, CPX_PARAM_TILIM, residual_time);

        if (inst->verbosity >= DEBUG) {
            printf("\n--- Starting SEC iteration with existing: %d SECs---\n", secCount);
            printf("Elapsed time: %.2f s | Residual time: %.2f s\n", elapsed, residual_time);
        }

        if (CPXmipopt(env, lp)) {
            char errmsg[CPXMESSAGEBUFSIZE];
            CPXgeterrorstring(env, error, errmsg);
            print_error_and_die(errmsg);
        }

        if (CPXgetx(env, lp, xstar, 0, inst->ncols - 1)) {
            char errmsg[CPXMESSAGEBUFSIZE];
            CPXgeterrorstring(env, error, errmsg);
            print_error_and_die(errmsg);
        }

        //Debug (Can be removed)
        // for (int i = 0; i < inst->nnodes; i++) {
        //     for (int j = i + 1; j < inst->nnodes; j++) {
        //         if (xstar[xpos(i, j, inst)] > 0.5) printf("  ... x(%3d,%3d) = 1\n", i + 1, j + 1);
        //     }
        // }

        build_sol(xstar, inst, succ, comp, ncomp);

        //Debug
        //plot_solution_edges(inst, succ);

        if (*ncomp > 1) {
            int *compCounts = (int *)calloc(*ncomp, sizeof(int));
            for (int i = 0; i < inst->nnodes; i++) {
                compCounts[comp[i] - 1]++;
            }

            for (int k = 0; k < *ncomp; k++) {
                int nnz = 0;
                int num_nodes_in_s = compCounts[k];

                if (num_nodes_in_s >= inst->nnodes) continue;

                double rhs = (double)num_nodes_in_s - 1.0;
                char sense = 'L';

                for (int i = 0; i < inst->nnodes; i++) {
                    if (comp[i] - 1 != k) continue;

                    for (int j = i + 1; j < inst->nnodes; j++) {
                        if (comp[j] - 1 != k) continue;

                        index[nnz] = index_upper(i, j, inst->nnodes);
                        value[nnz] = 1.0;
                        nnz++;
                    }
                }

                sprintf(cname[0], "SEC_%d", ++secCount);

                int status = CPXaddrows(env, lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, cname);
                if (status) {
                    char errmsg[CPXMESSAGEBUFSIZE];
                    CPXgeterrorstring(env, status, errmsg);
                    print_error_and_die(errmsg);
                }
            }
            free(compCounts);
        }

        if (second() - inst->tstart > inst->timelimit) break;

    } while (*ncomp > 1);
}

//Callback for integer candidates
static void post_heuristic_solution(CPXCALLBACKCONTEXTptr context, instance *inst, const int *succ, const int *comp, int ncomp, double objval) {
    if (inst->verbosity >= DEBUG) {
        printf("\n[DEBUG-HEUR] Entering post_heuristic_solution with ncomp = %d, current objval = %.2lf\n", ncomp, objval);
    }

    // NOTE: Patching heuristic integration
    int *succ_patched = (int *) malloc(inst->nnodes * sizeof(int));
    int *comp_patched = (int *) malloc(inst->nnodes * sizeof(int));
    for (int i = 0; i < inst->nnodes; i++) {
        succ_patched[i] = succ[i];
        comp_patched[i] = comp[i]; // NOTE: Deep copy of the actual components found by build_sol
    }
    int ncomp_patched = ncomp;

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Starting apply_patching...\n");
    }

    // NOTE: Setup current objective value and update it directly during patching
    double zbest_2opt = objval;
    apply_patching(inst, succ_patched, comp_patched, &ncomp_patched, &zbest_2opt);

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Finished apply_patching. Remaining components: %d, Patched cost: %.2lf\n", ncomp_patched, zbest_2opt);
        printf("[DEBUG-HEUR] Starting succ_to_permutation...\n");
    }

    // NOTE: Convert from successor array to permutation array
    unsigned int *sol_perm = (unsigned int *) malloc(inst->nnodes * sizeof(unsigned int));
    succ_to_permutation(inst->nnodes, succ_patched, sol_perm);

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Finished succ_to_permutation.\n");
    }

    // NOTE: Dynamic calculation of residual time limit inside the callback
    double elapsed = second() - inst->tstart;
    double remaining_time = inst->timelimit - elapsed;
    if (remaining_time < 0) remaining_time = 0.0;

    double tbest_2opt = elapsed;

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Time elapsed: %.2lf, Remaining time: %.2lf\n", elapsed, remaining_time);
        printf("[DEBUG-HEUR] Starting twoOpt_refinement...\n");
    }

    // NOTE: Apply 2-opt refinement strategy to the patched single tour only if time permits
    if (remaining_time > 0.0) {
        twoOpt_refinement(inst, remaining_time, sol_perm, &zbest_2opt, &tbest_2opt,0);
    }

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Finished twoOpt_refinement. Refined cost (zbest_2opt): %.2lf\n", zbest_2opt);
        printf("[DEBUG-HEUR] Starting permutation_to_succ...\n");
    }

    // NOTE: Convert back from permutation array to successor array
    permutation_to_succ(inst->nnodes, sol_perm, succ_patched);

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Finished permutation_to_succ. Building xheu...\n");
    }

    double *xheu = (double *) calloc(inst->ncols, sizeof(double));
    double objheu = 0.0;

    for (int i = 0; i < inst->nnodes; i++) {
        int j = succ_patched[i];
        int idx = xpos(i, j, inst); // xpos() sorts (i,j); index_upper requires i<j (C1 fix)
        xheu[idx] = 1.0;
        objheu += get_precalculated_distance(inst, i, j);
    }

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] xheu built. Recalculated cost (objheu): %.2lf\n", objheu);
    }

    if (objheu < CPX_INFBOUND) {
        int *ind = (int *) malloc(inst->ncols * sizeof(int));
        for (int j = 0; j < inst->ncols; j++) ind[j] = j;

        if (inst->verbosity >= DEBUG) {
            printf("[DEBUG-HEUR] Attempting CPXcallbackpostheursoln...\n");
        }

        if (CPXcallbackpostheursoln(context, inst->ncols, ind, xheu, objheu, CPXCALLBACKSOLUTION_NOCHECK)) {
            print_error_and_die("CPXcallbackpostheursoln() error");
        } else {
            if (inst->verbosity >= INFO) {
                printf("[CPLEX Callback] Successfully posted heuristic solution! Cost: %.2lf\n", objheu);
            }
        }

        free(ind);
    }

    if (inst->verbosity >= DEBUG) {
        printf("[DEBUG-HEUR] Freeing memory and exiting post_heuristic_solution.\n\n");
    }

    free(xheu);
    free(sol_perm);
    free(succ_patched);
    free(comp_patched);
}

static int add_subtour_elimination_constraints(CPXCALLBACKCONTEXTptr context, instance *inst, const int *comp, int ncomp) {
    int error = 0;

    int *compCounts = (int *) calloc(ncomp, sizeof(int));
    for (int i = 0; i < inst->nnodes; i++) {
        compCounts[comp[i] - 1]++;
    }

    int max_nnz = (inst->nnodes * (inst->nnodes - 1)) / 2;
    int *index = (int *) malloc(max_nnz * sizeof(int));
    double *value = (double *) malloc(max_nnz * sizeof(double));
    int izero = 0;

    for (int k = 0; k < ncomp; k++) {
        int nnz = 0;
        int num_nodes_in_s = compCounts[k];

        if (num_nodes_in_s >= inst->nnodes) continue;

        double rhs = (double)num_nodes_in_s - 1.0;
        char sense = 'L';

        for (int i = 0; i < inst->nnodes; i++) {
            if (comp[i] - 1 != k) continue;

            for (int j = i + 1; j < inst->nnodes; j++) {
                if (comp[j] - 1 != k) continue;

                index[nnz] = index_upper(i, j, inst->nnodes);
                value[nnz] = 1.0;
                nnz++;
            }
        }

        int max_nnz_current_comp = (num_nodes_in_s * (num_nodes_in_s - 1)) / 2;

        if (nnz > max_nnz_current_comp) {
            char error_msg[256];
            sprintf(error_msg,
                    "Critical Error in add_subtour_elimination_constraints:\n"
                    "  [Component k]: %d\n"
                    "  [Nodes in S]: %d\n"
                    "  [Current nnz]: %d\n"
                    "  [Max allowed nnz]: %d\n"
                    "The number of non-zeros exceeds the geometric limit of the component.",
                    k, num_nodes_in_s, nnz, max_nnz_current_comp);

            print_error_and_die(error_msg);
        }

        if (nnz > 0) {
            error = CPXcallbackrejectcandidate(context, 1, nnz, &rhs, &sense, &izero, index, value);
            if (error) break;
        }
    }

    free(index);
    free(value);
    free(compCounts);

    return error;
}

static int my_callback_candidate(CPXCALLBACKCONTEXTptr context, instance *inst) {
    int error = 0;

    double *xstar = (double *) malloc(inst->ncols * sizeof(double));
    double objval = CPX_INFBOUND;

    // NOTE: Get xstar from cplex
    if (CPXcallbackgetcandidatepoint(context, xstar, 0, inst->ncols - 1, &objval)) {
        free(xstar);
        return 1;
    }

    int *succ = (int *) malloc(inst->nnodes * sizeof(int));
    int *comp = (int *) malloc(inst->nnodes * sizeof(int));
    int ncomp;

    build_sol(xstar, inst, succ, comp, &ncomp);

    if (ncomp > 1) {
        // NOTE: Run and post the heuristic solution first
        post_heuristic_solution(context, inst, succ, comp, ncomp, objval);

        // NOTE: Execute the usual SEC cuts to reject the current invalid candidate
        error = add_subtour_elimination_constraints(context, inst, comp, ncomp);
    }

    free(succ);
    free(comp);
    free(xstar);

    return error;
}

//NOTE: Implement fractional solution
static int my_callback_relaxation(CPXCALLBACKCONTEXTptr context, instance *inst) {
    int error = 0;

    //NOTE: Extracting current solution
    double *xstar = (double *) malloc(inst->ncols * sizeof(double));
    double objval = CPX_INFBOUND;

    if (CPXcallbackgetrelaxationpoint(context, xstar, 0, inst->ncols - 1, &objval)) {
        free(xstar);
        return 0;
    }

    //NOTE: Counting active arcs and defining data structures
    int edge_cuts_counter = 0;
    for (int i = 0; i < inst->ncols; i++) {
        //TODO: Defining a global Epsilon
        if (xstar[i] > 1e-6) edge_cuts_counter++;
    }

    // C10 fix: nothing to separate if there are no active edges (avoids malloc(0) UB and a CCcut_mincut on empty input).
    if (edge_cuts_counter == 0) {
        free(xstar);
        return 0;
    }

    //NOTE: Defining data structure for concorde lib
    int *elist = (int *) malloc(2 * edge_cuts_counter * sizeof(int));
    double *xlist = (double *) malloc(edge_cuts_counter * sizeof(double));

    int edge_idx = 0;
    for (int i = 0; i < inst->nnodes; i++) {
        for (int j = i + 1; j < inst->nnodes; j++) {
            unsigned int idx = index_upper(i, j, inst->nnodes);
            if (xstar[idx] > 1e-6) {
                elist[2 * edge_idx]     = i;
                elist[2 * edge_idx + 1] = j;
                xlist[edge_idx]         = xstar[idx];
                edge_idx++;
            }
        }
    }

    //NOTE: Concorde computation
    int *cut = NULL;
    int cutcount = 0;
    double min_cut_val;

    int status = CCcut_mincut(inst->nnodes,  edge_cuts_counter, elist, xlist, &min_cut_val, &cut, &cutcount);

    //NOTE: Adding SEC for this weak component (Same logic as previous SECs)
    if (status == 0 && min_cut_val < 2.0 - 1e-4 && cutcount >= 2 && cutcount <= inst->nnodes - 2) {

        int max_nnz = (inst->nnodes * (inst->nnodes - 1)) / 2;
        int *index = (int *) malloc(max_nnz * sizeof(int));
        double *value = (double *) malloc(max_nnz * sizeof(double));
        int nnz = 0;

        double rhs = (double)cutcount - 1.0;
        char sense = 'L';
        int purgeable = CPX_USECUT_FILTER;

        char *in_s = (char *) calloc(inst->nnodes, sizeof(char));
        for (int i = 0; i < cutcount; i++) {
            in_s[cut[i]] = 1;
        }

        for (int i = 0; i < inst->nnodes; i++) {
            if (!in_s[i]) continue;
            for (int j = i + 1; j < inst->nnodes; j++) {
                if (!in_s[j]) continue;

                index[nnz] = index_upper(i, j, inst->nnodes);
                value[nnz] = 1.0;
                nnz++;
            }
        }

        if (nnz > 0) {
            int rmatbeg = 0;
            int local = 0;
            error = CPXcallbackaddusercuts(context, 1, nnz, &rhs, &sense, &rmatbeg, index, value, &purgeable, &local);
        }

        free(in_s);
        free(index);
        free(value);
    }

    // NOTE: CC_IFFREE è una macro interna di Concorde per liberare la memoria allocata dalla libreria
    if (cut) CC_IFFREE(cut, int);
    free(elist);
    free(xlist);
    free(xstar);

    return error;
}

//NOTE: Callbacks driver
static int CPXPUBLIC my_callback(CPXCALLBACKCONTEXTptr context, CPXLONG contextid, void *userhandle) {
    instance* inst = (instance*) userhandle;

    if (contextid == CPX_CALLBACKCONTEXT_CANDIDATE) {
        return my_callback_candidate(context, inst);
    }
    if (contextid == CPX_CALLBACKCONTEXT_RELAXATION) {
        return my_callback_relaxation(context, inst);
    }

    return 0;
}

static void TSPopt_callbacks(instance *inst, CPXENVptr env, CPXLPptr lp, double *xstar,
                             int *succ, int *comp, int *ncomp) {
    int error = 0;

    // Configurazione del contesto per ricevere le Lazy Constraints (Candidati interi)
    CPXLONG contextid = CPX_CALLBACKCONTEXT_CANDIDATE | CPX_CALLBACKCONTEXT_RELAXATION;

    if (CPXcallbacksetfunc(env, lp, contextid, my_callback, inst)) {
        print_error_and_die("Error in CPXcallbacksetfunc()");
    }

    // C2 fix: the candidate callback runs a heuristic that mutates shared `inst` state
    // (inst->iterations, and tabulist/plot when enabled). Force a single search thread so
    // concurrent callbacks cannot race on it. Trades parallel speed for correctness.
    CPXsetintparam(env, CPX_PARAM_THREADS, 1);

    // Calcolo e impostazione del time limit residuo iniziale
    double residual_time = time_left(inst); // T4 fix: shared residual helper (already clamped >= 0)

    CPXsetdblparam(env, CPX_PARAM_TILIM, residual_time);

    if (inst->verbosity >= DEBUG) {
        printf("\n[CPLEX] Starting optimization branch-and-cut with generic callbacks.\n");
    }

    if (CPXmipopt(env, lp)) {
        char errmsg[CPXMESSAGEBUFSIZE];
        CPXgeterrorstring(env, error, errmsg);
        print_error_and_die(errmsg);
    }

    //NOTE: Extract the CPLEX best sol.
    if (CPXgetx(env, lp, xstar, 0, inst->ncols - 1)) {
        if (inst->verbosity >= SILENT) {
            printf("Warning: No feasible solution found or error retrieving xstar.\n");
        }

        //NOTE: Force patching heuristic execution
        *ncomp = inst->nnodes;
        return;
    }

    build_sol(xstar, inst, succ, comp, ncomp);
}

static void TSPopt_hard_fixing(instance *inst, CPXENVptr env, CPXLPptr lp) {
    int error = 0;
    int iteration = 0;
    double p = 0.5;

    // NOTE: Register the generic callbacks to handle SECs inside the sub-problems
    CPXLONG contextid = CPX_CALLBACKCONTEXT_CANDIDATE | CPX_CALLBACKCONTEXT_RELAXATION;
    if (CPXcallbacksetfunc(env, lp, contextid, my_callback, inst)) {
        print_error_and_die("Error in CPXcallbacksetfunc() inside Hard Fixing");
    }
    CPXsetintparam(env, CPX_PARAM_THREADS, 1); // C2 fix: avoid races on shared inst from parallel callbacks

    // NOTE: Arrays required for CPXchgbds allocated once outside the loop
    int *indices_to_change = (int *) malloc(inst->nnodes * sizeof(int));
    char *lu_bounds = (char *) malloc(inst->nnodes * sizeof(char));
    double *bd_values = (double *) malloc(inst->nnodes * sizeof(double));

    // NOTE: Allocation for temporary solution extraction inside the loop
    int *succ = (int *) calloc(inst->nnodes, sizeof(int));
    int *comp = (int *) calloc(inst->nnodes, sizeof(int));
    int ncomp = 0;
    double *xstar = (double *) calloc(inst->ncols, sizeof(double));

    // M3 fix: hoist the fixed-size MIP-start buffers out of the loop (were alloc/free'd every iteration).
    // mipstart_ind is the constant column list 0..ncols-1; x_start_clique is rebuilt (memset) each iteration.
    double *x_start_clique = (double *) calloc(inst->ncols, sizeof(double));
    int *mipstart_ind = (int *) malloc(inst->ncols * sizeof(int));
    for (int i = 0; i < inst->ncols; i++) mipstart_ind[i] = i;

    if (inst->verbosity >= INFO) {
        printf("\n[Hard-Fixing] Starting Matheuristic loop. Global Time Limit: %.2lf s\n", inst->timelimit);
    }

    // MAIN LOOP: Run until time limit expires
    while (1) {
        double residual_time = time_left(inst); // T4 fix: shared residual helper + TIME_EPS guard

        if (residual_time <= TIME_EPS) {
            if (inst->verbosity >= INFO) printf("[Hard-Fixing] Time limit reached. Exiting loop.\n");
            break;
        }

        // 1. Update the fixing probability p every 10 iterations
        update_fixing_probability(&p, iteration, inst);

        // 2. Inject the current incumbent into CPLEX via MIPSTART

        // NOTE: First delete any existing MIP starts from previous iterations to avoid accumulation
        int num_existing_mipstarts = CPXgetnummipstarts(env, lp);
        if (num_existing_mipstarts > 0) {
            CPXdelmipstarts(env, lp, 0, num_existing_mipstarts - 1);
        }

        memset(x_start_clique, 0, inst->ncols * sizeof(double)); // M3: reset the hoisted buffer
        for (int i = 0; i < inst->nnodes; i++) {
            int curr = inst->solution[i];
            int next = inst->solution[(i + 1) % inst->nnodes];
            int idx = xpos(curr, next, inst); // xpos() sorts the pair (C1/HF1 fix)
            x_start_clique[idx] = 1.0;
        }

        // NOTE: Correct standard C macro is CPX_MIPSTART_AUTO (value 0)
        int mipstart_effort = CPX_MIPSTART_AUTO;
        int beg = 0;
        CPXaddmipstarts(env, lp, 1, inst->ncols, &beg, mipstart_ind, x_start_clique, &mipstart_effort, NULL);

        // 3. Fix variables with probability p based on the current permutation
        int fixed_edges_count = 0;
        for (int i = 0; i < inst->nnodes; i++) {
            int curr = inst->solution[i];
            int next = inst->solution[(i + 1) % inst->nnodes];

            if (((double)rand() / RAND_MAX) < p) {
                int idx = xpos(curr, next, inst); // xpos() sorts the pair (C1/HF1 fix)
                indices_to_change[fixed_edges_count] = idx;
                lu_bounds[fixed_edges_count] = 'L';
                bd_values[fixed_edges_count] = 1.0;
                fixed_edges_count++;
            }
        }

        if (fixed_edges_count > 0) {
            CPXchgbds(env, lp, fixed_edges_count, indices_to_change, lu_bounds, bd_values);
        }

        if (inst->verbosity >= DEBUG) {
            printf("[Hard-Fixing] Iteration %d: Fixed %d edges out of %d. Residual time: %.2lf s\n",
                   iteration, fixed_edges_count, inst->nnodes, residual_time);
        }

        // 4. Solve the restricted model with CPLEX.
        // T2 fix: bound each restricted MIP's time (aim for ~10 dives over a finite budget, 1s floor)
        // instead of handing the whole residual to a single solve.
        double per_iter = residual_time;
        if (inst->timelimit < 1e15) {
            double cap = inst->timelimit / 10.0;
            if (cap < per_iter) per_iter = cap;
        }
        if (per_iter < 1.0 && residual_time > 1.0) per_iter = 1.0;
        CPXsetdblparam(env, CPX_PARAM_TILIM, per_iter);

        if (CPXmipopt(env, lp)) {
            if (inst->verbosity >= DEBUG) printf("Warning: CPXmipopt failed or subproblem infeasible at iteration %d.\n", iteration);
        } else {
            // 5. If CPLEX found a solution, extract it and check for improvements
            if (!CPXgetx(env, lp, xstar, 0, inst->ncols - 1)) {
                build_sol(xstar, inst, succ, comp, &ncomp);

                double sub_objval;
                CPXgetobjval(env, lp, &sub_objval);

                if (sub_objval < inst->zbest) {
                    inst->zbest = sub_objval;
                    inst->tbest = second() - inst->tstart; // T5 fix: record time of best
                    succ_to_permutation(inst->nnodes, succ, inst->solution);
                    if (inst->verbosity >= INFO) {
                        printf("[Hard-Fixing] New global incumbent found! Cost: %.2lf at time %.2lf s\n", sub_objval, second() - inst->tstart);
                    }
                }
            }
        }

        // 6. Unfix all variables (Restore lower bounds back to 0.0)
        if (fixed_edges_count > 0) {
            for (int i = 0; i < fixed_edges_count; i++) {
                bd_values[i] = 0.0;
            }
            CPXchgbds(env, lp, fixed_edges_count, indices_to_change, lu_bounds, bd_values);
        }

        iteration++;
    }

    // NOTE: Clean up allocations
    free(indices_to_change);
    free(lu_bounds);
    free(bd_values);
    free(succ);
    free(comp);
    free(xstar);
    free(x_start_clique); // M3: hoisted MIP-start buffers freed once
    free(mipstart_ind);
}

// region Local Branching (matheuristic)

// Tunable parameters for the local-branching matheuristic.
#define LB_K_BASE        10     // initial neighborhood radius (max incumbent edges allowed to change)
#define LB_K_STEP        10     // amount k is widened when the current neighborhood is exhausted
#define LB_K_MAX         50     // hard cap on k (also capped by n-1)
#define LB_ITERS_TARGET  10.0   // aim for ~this many dives over a finite time budget
#define LB_MIN_ITER_TIME 1.0    // floor on per-iteration CPLEX time (s)
#define LB_TIME_EPS      0.1    // stop when residual time drops below this (s)

/**
 * @brief Injects the current incumbent tour (inst->solution) into CPLEX as a MIP start (warm start).
 *        Uses xpos() so the edge->column mapping is correct regardless of node ordering.
 */
static void inject_incumbent_mipstart(instance *inst, CPXENVptr env, CPXLPptr lp) {
    int num_existing = CPXgetnummipstarts(env, lp);
    if (num_existing > 0) CPXdelmipstarts(env, lp, 0, num_existing - 1);

    double *xstart = (double *) calloc(inst->ncols, sizeof(double));
    int *ind = (int *) malloc(inst->ncols * sizeof(int));
    for (int i = 0; i < inst->ncols; i++) ind[i] = i;

    for (int i = 0; i < inst->nnodes; i++) {
        int curr = inst->solution[i];
        int next = inst->solution[(i + 1) % inst->nnodes];
        xstart[xpos(curr, next, inst)] = 1.0; // xpos() sorts the pair -> correct column
    }

    int effort = CPX_MIPSTART_AUTO;
    int beg = 0;
    CPXaddmipstarts(env, lp, 1, inst->ncols, &beg, ind, xstart, &effort, NULL);

    free(ind);
    free(xstart);
}

/**
 * @brief Seeds inst->solution / inst->zbest with an NN + 2-opt heuristic tour.
 *        Shared warm-start for the CPLEX matheuristics (local branching here; hard-fixing can reuse it).
 */
static void matheuristic_warm_start(instance *inst) {
    // Force a plain NN + 2-opt configuration (no GRASP randomization, no kicks, no tabu).
    inst->GRASP_alpha = 0;
    inst->VNS_kick_counter = 0;
    inst->tenure = 0;

    int finite = (inst->timelimit < 1e15);
    double constr_budget = finite ? inst->timelimit * 0.1 : inst->timelimit;
    double refine_budget = finite ? inst->timelimit * 0.2 : inst->timelimit;

    greedy_nn_heuristic(inst, constr_budget, NONE);
    twoOpt_refinement(inst, refine_budget, inst->solution, &inst->zbest, &inst->tbest, 0);

    if (inst->verbosity >= INFO) {
        printf("[Matheuristic] Warm-start (NN+2opt) cost: %.2lf at %.2lf s\n",
               inst->zbest, second() - inst->tstart);
    }
}

/**
 * @brief Local-branching matheuristic (Fischetti-Lodi soft fixing).
 *
 * Keeps the full TSP model and, around the current incumbent tour x_bar, adds the soft-fixing constraint
 *     sum_{e in E(x_bar)} x_e >= n - k
 * which allows at most k of the n incumbent edges to leave the tour (neighborhood radius k). The restricted
 * problem is solved (under SEC callbacks) within a bounded per-iteration time slice; on improvement the
 * incumbent is re-centered, otherwise the neighborhood is widened (k += step). The constraint is removed and
 * rebuilt each iteration. Requires inst->solution/inst->zbest to already hold a feasible warm-start tour.
 */
static void TSPopt_local_branching(instance *inst, CPXENVptr env, CPXLPptr lp) {
    // Register SEC separation callbacks for the sub-MIPs (same as the other CPLEX modes).
    CPXLONG contextid = CPX_CALLBACKCONTEXT_CANDIDATE | CPX_CALLBACKCONTEXT_RELAXATION;
    if (CPXcallbacksetfunc(env, lp, contextid, my_callback, inst)) {
        print_error_and_die("Error in CPXcallbacksetfunc() inside Local Branching");
    }
    CPXsetintparam(env, CPX_PARAM_THREADS, 1); // C2 fix: avoid races on shared inst from parallel callbacks

    const int n = inst->nnodes;

    // Buffers allocated once and reused across iterations.
    int *succ = (int *) calloc(n, sizeof(int));
    int *comp = (int *) calloc(n, sizeof(int));
    double *xstar = (double *) calloc(inst->ncols, sizeof(double));
    int *lb_index = (int *) malloc(n * sizeof(int));   // columns of the incumbent edges
    double *lb_value = (double *) malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) lb_value[i] = 1.0;

    int k = LB_K_BASE;
    int k_max = (n - 1 < LB_K_MAX) ? (n - 1) : LB_K_MAX;   // keep rhs = n-k >= 1
    if (k_max < LB_K_BASE) k_max = LB_K_BASE;              // tiny instances: allow at least one solve
    int iteration = 0;

    if (inst->verbosity >= INFO) {
        printf("\n[Local-Branching] Start. Global time limit %.2lf s, base k=%d, k_max=%d\n",
               inst->timelimit, k, k_max);
    }

    while (1) {
        double elapsed = second() - inst->tstart;
        double residual = inst->timelimit - elapsed;
        if (residual <= LB_TIME_EPS) {
            if (inst->verbosity >= INFO) printf("[Local-Branching] Time limit reached. Exiting loop.\n");
            break;
        }
        if (k > k_max) {
            if (inst->verbosity >= INFO) printf("[Local-Branching] Neighborhood exhausted (k>%d). Exiting loop.\n", k_max);
            break;
        }

        // Bounded per-iteration time slice (capped diving instead of spending the whole residual at once).
        double per_iter = residual;
        if (inst->timelimit < 1e15) {
            double cap = inst->timelimit / LB_ITERS_TARGET;
            if (cap < per_iter) per_iter = cap;
        }
        if (per_iter < LB_MIN_ITER_TIME && residual > LB_MIN_ITER_TIME) per_iter = LB_MIN_ITER_TIME;
        if (per_iter > residual) per_iter = residual;

        // Warm-start the sub-MIP with the current incumbent.
        inject_incumbent_mipstart(inst, env, lp);

        // Build and add the local-branching constraint: sum_{e in incumbent} x_e >= n - k.
        for (int i = 0; i < n; i++) {
            int curr = inst->solution[i];
            int next = inst->solution[(i + 1) % n];
            lb_index[i] = xpos(curr, next, inst);
        }
        double rhs = (double)(n - k);
        char sense = 'G';
        int izero = 0;
        int rows_before = CPXgetnumrows(env, lp);
        if (CPXaddrows(env, lp, 0, 1, n, &rhs, &sense, &izero, lb_index, lb_value, NULL, NULL)) {
            print_error_and_die("CPXaddrows() failed for local-branching constraint");
        }

        CPXsetdblparam(env, CPX_PARAM_TILIM, per_iter);

        if (inst->verbosity >= DEBUG) {
            printf("[Local-Branching] iter %d: k=%d, rhs=%.0f, slice=%.2fs, residual=%.2fs\n",
                   iteration, k, rhs, per_iter, residual);
        }

        int solve_err = CPXmipopt(env, lp);
        int improved = 0;
        if (!solve_err) {
            int stat = CPXgetstat(env, lp);
            if (CPXgetx(env, lp, xstar, 0, inst->ncols - 1) == 0) {
                int ncomp = 0;
                build_sol(xstar, inst, succ, comp, &ncomp);
                if (ncomp == 1) {
                    unsigned int *cand = (unsigned int *) malloc(n * sizeof(unsigned int));
                    succ_to_permutation(n, succ, cand);
                    double cand_cost = calculate_solution_cost(inst, cand);
                    if (cand_cost < inst->zbest - EPSILON) {
                        inst->zbest = cand_cost;
                        inst->tbest = second() - inst->tstart;
                        memcpy(inst->solution, cand, sizeof(unsigned int) * n);
                        improved = 1;
                        if (inst->verbosity >= INFO) {
                            printf("[Local-Branching] New incumbent: cost %.2lf at %.2lf s (k=%d)\n",
                                   inst->zbest, inst->tbest, k);
                        }
                    }
                    free(cand);
                } else if (inst->verbosity >= DEBUG) {
                    printf("[Local-Branching] Sub-MIP returned %d components; skipping (callbacks should prevent this).\n", ncomp);
                }
            }

            // Adapt the neighborhood radius.
            if (improved) {
                k = LB_K_BASE;            // re-center and intensify around the new incumbent
            } else if (stat == CPXMIP_OPTIMAL || stat == CPXMIP_OPTIMAL_TOL) {
                k += LB_K_STEP;           // current neighborhood fully explored with no gain -> widen
            }
            // else: timed out without improvement -> keep k and continue with remaining time.
        }

        // Remove the local-branching row before the next iteration (soft fixing is transient).
        CPXdelrows(env, lp, rows_before, rows_before);
        iteration++;
    }

    if (inst->verbosity >= INFO) {
        printf("[Local-Branching] Finished after %d iterations. Best cost: %.2lf\n", iteration, inst->zbest);
    }

    free(succ);
    free(comp);
    free(xstar);
    free(lb_index);
    free(lb_value);
}

// endregion Local Branching

int TSPopt(instance *inst, enum CPLEX_MODE mode) {
    int error = 0;

    CPXENVptr env = CPXopenCPLEX(&error);
    if (error || env == NULL) {
        char errmsg[CPXMESSAGEBUFSIZE];
        CPXgeterrorstring(env, error, errmsg);
        print_error_and_die(errmsg);
    }

    CPXLPptr lp = CPXcreateprob(env, &error, "TSP");
    if (error || lp == NULL) {
        char errmsg[CPXMESSAGEBUFSIZE];
        CPXgeterrorstring(env, error, errmsg);
        CPXcloseCPLEX(&env); // C6 fix: release the env (and its license token) before aborting
        print_error_and_die(errmsg);
    }

    build_model(inst, env, lp);
    inst->ncols = CPXgetnumcols(env, lp);

    // NOTE: Memory allocation and post-processing variables
    int *succ = NULL;
    int *comp = NULL;
    int ncomp = 0;

    switch (mode) {
        case BENDERS_LOOP: {
            // NOTE: Allocate memory exclusively needed for Benders Loop
            succ = (int *) calloc(inst->nnodes, sizeof(int));
            comp = (int *) calloc(inst->nnodes, sizeof(int));
            double *xstar = (double *) calloc(inst->ncols, sizeof(double));

            int max_nnz = (inst->nnodes * (inst->nnodes - 1)) / 2;
            int *index = (int *) calloc(max_nnz, sizeof(int));
            double *value = (double *) calloc(max_nnz, sizeof(double));

            char **cname = (char **) calloc(1, sizeof(char *));
            cname[0] = (char *) calloc(100, sizeof(char));

            TSPopt_benders(inst, env, lp, xstar, succ, comp, &ncomp, index, value, cname);

            // NOTE: Free memory immediately after method execution
            free(cname[0]);
            free(cname);
            free(index);
            free(value);
            free(xstar);
            break;
        }

        case CALLBACKS: {
            // NOTE: Allocate memory exclusively needed for generic Callbacks
            succ = (int *) calloc(inst->nnodes, sizeof(int));
            comp = (int *) calloc(inst->nnodes, sizeof(int));
            double *xstar = (double *) calloc(inst->ncols, sizeof(double));

            TSPopt_callbacks(inst, env, lp, xstar, succ, comp, &ncomp);

            free(xstar);
            break;
        }

        case HARD_FIXING: {
            // C5 fix: seed a real feasible incumbent (NN + 2-opt) before diving, instead of
            // starting from the identity permutation.
            matheuristic_warm_start(inst);
            TSPopt_hard_fixing(inst, env, lp);
            break;
        }

        case LOCAL_BRANCHING: {
            // Seed a feasible incumbent (NN + 2-opt) before soft-fixing around it.
            matheuristic_warm_start(inst);
            TSPopt_local_branching(inst, env, lp);
            break;
        }

        default:
            print_error_and_die("Unknown CPLEX mode selected.");
    }

    // NOTE: Post-processing and cleanup (Only for exact/heuristic methods that populate succ and ncomp)
    if (mode == BENDERS_LOOP || mode == CALLBACKS) {
        if (ncomp > 1) {
            // WARNING: Check if succ contains a valid composition of nodes
            int is_succ_all_zeros = 1;
            for (int i = 0; i < inst->nnodes; i++) {
                if (succ[i] != 0) {
                    is_succ_all_zeros = 0;
                    break;
                }
            }

            if (is_succ_all_zeros) {
                print_error_and_die("Error: CPLEX execution was interrupted before finding any solution. 'succ' array is uninitialized.");
            }

            // NOTE: Apply patching heuristic and update zbest inside the method
            apply_patching(inst, succ, comp, &ncomp, &(inst->zbest));

        } else {
            double objval;
            if (CPXgetobjval(env, lp, &objval)) {
                char errmsg[CPXMESSAGEBUFSIZE];
                CPXgeterrorstring(env, error, errmsg);
                print_error_and_die(errmsg);
            }
            inst->zbest = objval;
        }

        // NOTE: Convert from successor array to the final instance permutation solution
        succ_to_permutation(inst->nnodes, succ, inst->solution);
        inst->zbest = calculate_solution_cost(inst, inst->solution);
        inst->tbest = second() - inst->tstart; // T5 fix: record time of best for benders/callbacks

        // NOTE: Final clean up for exact methods arrays
        free(succ);
        free(comp);
    }

    CPXfreeprob(env, &lp);
    CPXcloseCPLEX(&env);

    return 0;
}


int xpos(int i, int j, instance *inst) {
    if (i == j) print_error_and_die(" i == j in xpos");
    if (i > j) return xpos(j, i, inst);
    return index_upper(i, j, inst->nnodes);
}

void build_model(instance *inst, CPXENVptr env, CPXLPptr lp) {
    int izero = 0;
    char binary = 'B';

    char **cname = (char **) calloc(1, sizeof(char *));
    cname[0] = (char *) calloc(100, sizeof(char));

    // add binary var.s x(i,j) for i < j
    for (int i = 0; i < inst->nnodes; i++) {
        for (int j = i + 1; j < inst->nnodes; j++) {
            sprintf(cname[0], "x(%d,%d)", i + 1, j + 1); // x(1,2), x(1,3) ....
            double obj = get_precalculated_distance(inst, i, j);
            double lb = 0.0;
            double ub = 1.0;
            int status = CPXnewcols(env, lp, 1, &obj, &lb, &ub, &binary, cname);
            if (status) {
                char errmsg[CPXMESSAGEBUFSIZE];
                CPXgeterrorstring(env, status, errmsg);
                print_error_and_die(errmsg);
            }

            if (CPXgetnumcols(env, lp) - 1 != xpos(i, j, inst)) print_error_and_die(" wrong position for x var.s");
        }
    }

    int *index = (int *) malloc(inst->nnodes * sizeof(int));
    double *value = (double *) malloc(inst->nnodes * sizeof(double));
    for (int h = 0; h < inst->nnodes; h++) // degree constraints
    {
        double rhs = 2.0;
        char sense = 'E'; // 'E' for equality constraint
        sprintf(cname[0], "degree(%d)", h + 1);
        int nnz = 0;
        for (int i = 0; i < inst->nnodes; i++) {
            if (i == h) continue;
            index[nnz] = xpos(i, h, inst);
            value[nnz] = 1.0;
            nnz++;
        }

        int status = CPXaddrows(env, lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, &cname[0]);
        if (status) {
            char errmsg[CPXMESSAGEBUFSIZE];
            CPXgeterrorstring(env, status, errmsg);
            print_error_and_die(errmsg);
        }
    }

    free(value);
    free(index);

    CPXwriteprob(env, lp, "model.lp", NULL);

    free(cname[0]);
    free(cname);
}




// C8 fix: renamed from DEBUG (which collided with the DEBUG verbosity constant in consts.h and
// triggered a -Wmacro-redefined warning). Comment this out to disable build_sol's degree checks.
#define BUILD_SOL_CHECKS    // da commentare se non si vuole il debugging
#define EPS 1e-5

/**
 * @brief  Builds the connected components (subtours) from the current solution vector.
 *
 * @param  xstar  Pointer to the continuous solution array (edge values).
 * @param  inst   Pointer to the instance structure containing graph details.
 * @param  succ   Array to store the successor of each node in the tour.
 * @param  comp   Array to map each node to its corresponding component ID.
 * @param  ncomp  Pointer to integer where the total number of components will be stored.
 */
void build_sol(const double *xstar, instance *inst, int *succ, int *comp, int *ncomp) {
#ifdef BUILD_SOL_CHECKS
    // Verify that every node has exactly degree 2 and variables are binary
    int *degree = (int *) calloc(inst->nnodes, sizeof(int));
    for (int i = 0; i < inst->nnodes; i++) {
        for (int j = i + 1; j < inst->nnodes; j++) {
            int k = xpos(i, j, inst);
            if (fabs(xstar[k]) > EPS && fabs(xstar[k] - 1.0) > EPS)
                print_error_and_die(" wrong xstar in build_sol()");
            if (xstar[k] > 0.5) {
                ++degree[i];
                ++degree[j];
            }
        }
    }
    for (int i = 0; i < inst->nnodes; i++) {
        if (degree[i] != 2) print_error_and_die("wrong degree in build_sol()");
    }
    free(degree);
#endif

    // Initialize components and successors arrays
    *ncomp = 0;
    for (int i = 0; i < inst->nnodes; i++) {
        succ[i] = -1;
        comp[i] = -1;
    }

    // Find and trace all connected components
    for (int start = 0; start < inst->nnodes; start++) {
        if (comp[start] >= 0) continue; // Skip already visited nodes

        // New component found
        (*ncomp)++;
        int i = start;
        int done = 0;

        while (!done) // Traverse the current component
        {
            comp[i] = *ncomp;
            done = 1;
            for (int j = 0; j < inst->nnodes; j++) {
                // If edge (i,j) is active and j is unvisited
                if (i != j && xstar[xpos(i, j, inst)] > 0.5 && comp[j] == -1) {
                    succ[i] = j;
                    i = j;
                    done = 0;
                    break;
                }
            }
        }
        succ[i] = start; // Close the subtour cycle
    }
}
