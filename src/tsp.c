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
                short res = twoOpt_refinement(inst, time_limit, temp_solution, &temp_solution_cost, &curr_time);
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
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short twoOpt_refinement(instance *inst, double time_limit, unsigned int *sol, double *zbest, double *tbest) {
    if (inst->verbosity >= DEBUG) printf("\t\t## STARTING twoOpt_refinement\n");

    short exitStatus = 0;

    double initial_cost = *zbest;
    double curr_time = second() - inst->tstart;


    if (inst->verbosity >= DEBUG)
        printf("\t\t -> 2-opt INIT with cost  %20lf at time %20lf\n", initial_cost,
               curr_time);

    unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
    memcpy(temp_solution, sol, sizeof(unsigned int) * inst->nnodes);

    plot_iteration(inst, initial_cost, *zbest < inst->zbest ? *zbest : inst->zbest, "2opt");


    while (1) {
        // unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
        // memcpy(temp_solution, sol, sizeof(unsigned int) * inst->nnodes);

        //NOTE: Print the starting cost (it's redundant for debug)
        if (inst->verbosity >= DEBUG)
            printf("\t\t -> 2-opt INIT with cost    %20lf at time %20lf\n", initial_cost,
                   curr_time);

        //NOTE: Tabu search is already implemented in this step due to parameter definition
        double temp_solution_cost_delta = twoOpt_refinement_iteration(inst, temp_solution);
        inst->iterations++; //NOTE: Update iterations after 2-opt !
        initial_cost += temp_solution_cost_delta;
        curr_time = second() - inst->tstart;
        if (inst->verbosity >= DEBUG)
            printf("\t\t -> STEP made with new cost %20lf at time %20lf\n", initial_cost,
                   curr_time);

        //Debug
        // if(inst->tenure>0 && temp_solution_cost_delta > 0)
        //     log_and_check_sol(inst, temp_solution, initial_cost);

        // plot_iteration(inst, initial_cost, *zbest < inst->zbest ? *zbest : inst->zbest, "2opt");

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
                    printf(
                        "------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------\n");
                    return -1;
                }
            }
        }

        plot_iteration(inst, initial_cost, *zbest < inst->zbest ? *zbest : inst->zbest, "2opt");

        //NOTE: If i'm in a local minima and NOT in tabu
        if (temp_solution_cost_delta >= -EPSILON && inst->tenure == 0) {
            if (inst->VNS_kick_counter == 0) {
                free(temp_solution);
                break;
            }
            // Vns kick
            for (int i = 0; i < inst->VNS_kick_counter; i++) {
                //NOTE: If the time is finished we break the cycle and the all refinement prob.
                int kick_status = vns_kick(inst, inst->timelimit, temp_solution, &initial_cost);

                //Debug
                // log_and_check_sol(inst, temp_solution, initial_cost);

                plot_iteration(inst, initial_cost, *zbest, "kick");

                if (kick_status == 1)
                    break;

                if (inst->verbosity >= DEBUG) {
                    printf("\t -> AFTER VNS Kick %10.2lf, time %8.4lf \n", initial_cost, curr_time);
                }
            }
        }


        //NOTE: Check the time limit
        if (time_limit != 0 && curr_time > time_limit) {
            exitStatus = 1;
            free(temp_solution);
            break;
        }

        plot_solution(inst, temp_solution, 0, 0);
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


int TSPopt(instance *inst) {
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

    // Cplex's parameter setting TODO!
    int *succ = (int *) malloc(inst->nnodes * sizeof(int));
    int *comp = (int *) malloc(inst->nnodes * sizeof(int));
    int ncols = CPXgetnumcols(env, lp);
    double *xstar = (double *) calloc(ncols, sizeof(double));
    int ncomp;

    //Used for SECs
    int secCount = 0;
    char **cname = (char **) calloc(1, sizeof(char *));
    cname[0] = (char *) calloc(100, sizeof(char));
    int izero = 0;

    int max_nnz = (inst->nnodes * (inst->nnodes - 1)) / 2;
    int *index = (int *) malloc(max_nnz * sizeof(int));
    double *value = (double *) malloc(max_nnz * sizeof(double));

    do
    {
        if (CPXmipopt(env, lp)) {
            char errmsg[CPXMESSAGEBUFSIZE];
            CPXgeterrorstring(env, error, errmsg);
            print_error_and_die(errmsg);
        }

        if (CPXgetx(env, lp, xstar, 0, ncols - 1)) {
            char errmsg[CPXMESSAGEBUFSIZE];
            CPXgeterrorstring(env, error, errmsg);
            print_error_and_die(errmsg);
        }

        for (int i = 0; i < inst->nnodes; i++) {
            for (int j = i + 1; j < inst->nnodes; j++) {
                if (xstar[xpos(i, j, inst)] > 0.5) printf("  ... x(%3d,%3d) = 1\n", i + 1, j + 1);
            }
        }
        
        build_sol(xstar, inst, succ, comp, &ncomp);

        //Debug
        plot_solution_edges(inst, succ);

        if (ncomp > 1)
        {
            int *compCounts = (int *)calloc(ncomp, sizeof(int));
            for (int i = 0; i < inst->nnodes; i++)
            {
                compCounts[comp[i] - 1]++;
            }

            for (int k = 0; k < ncomp; k++)
            {
                int nnz = 0;
                int num_nodes_in_s = compCounts[k];

                if (num_nodes_in_s >= inst->nnodes)
                    continue;

                double rhs = (double)num_nodes_in_s - 1.0;
                char sense = 'L';

                for (int i = 0; i < inst->nnodes; i++)
                {
                    if (comp[i] - 1 != k)
                        continue;

                    for (int j = i + 1; j < inst->nnodes; j++)
                    {
                        if (comp[j] - 1 != k)
                            continue;

                        //Adding all possible edges that i have and can have in this subtour
                        index[nnz] = index_upper(i, j, inst->nnodes);
                        value[nnz] = 1.0;
                        nnz++;
                    }
                }

                sprintf(cname[0], "SEC_%d", ++secCount);

                int status = CPXaddrows(env, lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, cname);
                if (status)
                {
                    char errmsg[CPXMESSAGEBUFSIZE];
                    CPXgeterrorstring(env, status, errmsg);
                    print_error_and_die(errmsg);
                }
            }
            free(compCounts);
        }

    } while (ncomp > 1);
    
    free(index);
    free(value);
    free(succ);
    free(comp);
    free(xstar);

    CPXfreeprob(env, &lp);
    CPXcloseCPLEX(&env);

    //Print solution

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

void build_sol(const double *xstar, instance *inst, int *succ, int *comp, int *ncomp) {
#ifdef DEBUG
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

    *ncomp = 0;
    for (int i = 0; i < inst->nnodes; i++) {
        succ[i] = -1;
        comp[i] = -1;
    }

    for (int start = 0; start < inst->nnodes; start++) {
        if (comp[start] >= 0) continue; // node "start" was already visited, just skip it

        // a new component is found
        (*ncomp)++;
        int i = start;
        int done = 0;
        while (!done) // go and visit the current component
        {
            comp[i] = *ncomp;
            done = 1;
            for (int j = 0; j < inst->nnodes; j++) {
                if (i != j && xstar[xpos(i, j, inst)] > 0.5 && comp[j] == -1)
                // the edge [i,j] is selected in xstar and j was not visited before
                {
                    succ[i] = j;
                    i = j;
                    done = 0;
                    break;
                }
            }
        }
        succ[i] = start; // last arc to close the cycle

        // go to the next component...
    }
}
