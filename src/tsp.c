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

//region Data-structure convertions

void succ_to_permutation(int nnodes, const int *succ, unsigned int *sol_perm) {
    int currNode = 0;
    for (int i = 0; i < nnodes; i++) {
        sol_perm[i] = (unsigned int) succ[currNode];
        currNode = succ[currNode];
    }
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

    if (expected_cost != calculate_solution_cost(inst, solution)) {
        printf("ERROR: Expected cost %lf, calculated cost %lf\n", expected_cost,
               calculate_solution_cost(inst, solution));
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
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
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
        double temp_solution_cost_delta = twoOpt_refinement_iteration(inst, temp_solution);
        inst->iterations++; // NOTE: Update iterations after 2-opt !
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
                if (inst->verbosity >= SILENT) {
                    printf("------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------\n");
                    free(temp_solution); // NOTE: Free memory before error exit to avoid leaks
                    return -1;
                }
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
                int kick_status = vns_kick(inst, inst->timelimit, temp_solution, &initial_cost);

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
 * @return double The improvement (reduction in cost) achieved by the iteration.
 */
double twoOpt_refinement_iteration(const instance *inst, unsigned int *sol) {
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

    if (inst->tenure > 0 && !isDeltaSetted) print_error_and_die("All moves are Tabu !");

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

    return minDeltaCost;
}


/**
 * @brief Applies the 3-opt kick by swapping 2 parts of the solution array
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
       
       Una classica 3-opt kick (Double Bridge) riassembla come: A -> D -> C -> B
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
        double residual_time = inst->timelimit - elapsed;

        if (residual_time <= 0) break;

        CPXsetdblparam(env, CPX_PARAM_TILIM, residual_time);

        if (inst->verbosity >= DEBUG) {
            printf("\n--- Starting SEC iteration %d ---\n", secCount + 1);
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
        int idx = index_upper(i, j, inst->nnodes);
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

    // Calcolo e impostazione del time limit residuo iniziale
    double elapsed = second() - inst->tstart;
    double residual_time = inst->timelimit - elapsed;
    if (residual_time < 0) residual_time = 0.0;

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
        print_error_and_die(errmsg);
    }

    build_model(inst, env, lp);

    int *succ = (int *) calloc(inst->nnodes, sizeof(int));
    int *comp = (int *) calloc(inst->nnodes, sizeof(int));
    inst->ncols = CPXgetnumcols(env, lp);
    double *xstar = (double *) calloc(inst->ncols, sizeof(double));
    int ncomp = 0;

    int max_nnz = (inst->nnodes * (inst->nnodes - 1)) / 2;
    int *index = (int *) calloc(max_nnz, sizeof(int));
    double *value = (double *) calloc(max_nnz, sizeof(double));

    char **cname = (char **) calloc(1, sizeof(char *));
    cname[0] = (char *) calloc(100, sizeof(char));

    // Gestione del flusso tramite lo switch dell'enumeratore richiesto
    switch (mode) {
        case BENDERS_LOOP:
            TSPopt_benders(inst, env, lp, xstar, succ, comp, &ncomp, index, value, cname);
            break;

        case CALLBACKS:
            TSPopt_callbacks(inst, env, lp, xstar, succ, comp, &ncomp);
            break;

        default:
            print_error_and_die("Unknown CPLEX mode selected.");
    }

    //NOTE: Managing time expiration or sub-optimal termination (Valid for both modes)
    if (ncomp > 1) {

        //WARNING: Check if succ contains a valid composition of nodes
        int is_succ_all_zeros = 1;
        for (int i = 0; i < inst->nnodes; i++) {
            if (succ[i] != 0) {
                is_succ_all_zeros = 0;
                break;
            }
        }

        if (is_succ_all_zeros) {
            print_error_and_die("Error: CPLEX execution was interrupted (e.g., time limit expired during breakpoint) before finding any solution. 'succ' array is uninitialized.");
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

    free(cname[0]);
    free(cname);
    free(index);
    free(value);
    free(succ);
    free(comp);
    free(xstar);

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




#define DEBUG    // da commentare se non si vuole il debugging
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
#ifdef DEBUG
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
