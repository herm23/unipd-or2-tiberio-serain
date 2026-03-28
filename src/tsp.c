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
    /* ----------- Basic error checking ----------- */
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

    /* ----------- Open gnuplot pipe ----------- */
    //@COMPLEX@: Opens a persistent pipe to gnuplot for visualization. This requires gnuplot to be installed and in the system PATH.

    if (inst->plot == NULL) {
        inst->plot = popen("gnuplot -persist", "w");
        if (inst->plot == NULL) {
            perror("plot_tour error: popen failed");
            return;
        }
        // fprintf(inst->plot, "set term qt\n");
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
        /* ---- Plot nodes + tour ---- */
        fprintf(inst->plot,
                "plot '-' with lines lw 2 lc rgb 'red', "
                "'-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        /* ---- Tour edges (closed) ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            int idx = solution[i];

            if (idx < 0 || idx >= inst->nnodes) {
                fprintf(stderr, "plot_tour error: invalid index in solution\n");
                pclose(inst->plot);
                return;
            }

            fprintf(inst->plot, "%f %f\n",
                    inst->xcoord[idx],
                    inst->ycoord[idx]);
        }

        /* Close the tour */
        int first = solution[0];
        fprintf(inst->plot, "%f %f\n",
                inst->xcoord[first],
                inst->ycoord[first]);

        fprintf(inst->plot, "e\n");

        /* ---- Nodes ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(inst->plot, "%f %f\n",
                    inst->xcoord[i],
                    inst->ycoord[i]);
        }

        fprintf(inst->plot, "e\n");
    }


    fflush(inst->plot);

    if (pause && inst->plot != NULL) {
        printf("\nPress ENTER to close the graph and resume the program...");
        getchar();
    }
}

//endregion Plots

/**
 * @brief Checks if a solution represents a valid tour (Hamiltonian cycle).
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 1 if the tour is valid, 0 otherwise.
 */
short is_solution_valid(const instance *inst, const unsigned int *solution) {
    //Return 0 if valid, -1 otherways
    if (inst == NULL || solution == NULL) return -1;

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
        cost += calculate_distance(sol[i], sol[i + 1], inst);

    cost += calculate_distance(sol[N - 1], sol[0], inst);
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
 * @param GRASP_alpha The alpha parameter for the GRASP heuristic (0 = greedy, 1 = random).
 * @param refinement The refinement strategy to be applied after each iteration.
 * @return short Returns 0 on success, 1 if the time limit was exceeded.
 */
short greedy_nn_heuristic(instance *inst, double time_limit, double GRASP_alpha,
                          const enum TSP_REFINEMENT_STRATEGY refinement) {
    printf("\t## START greedy_nn_heuristic\n");

    short exitStatus = 0;

    unsigned int *nodes_order = initialize_identity_solution(inst->nnodes);
    random_array_shuffle(nodes_order, inst->nnodes);

    for (int i = 0; i < inst->nnodes; i++) {
        unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
        double temp_solution_cost = greedy_nn_heuristic_iteration(inst, nodes_order[i], GRASP_alpha, temp_solution);

        double curr_time = second() - inst->tstart;
        printf("\t -> S-node %7d,  cost %10.2lf, time %8.2lf", nodes_order[i], temp_solution_cost,
               curr_time);

        if (refinement == TWO_OPT) {
            printf("\n");
            twoOpt_refinement(inst, time_limit, temp_solution, &temp_solution_cost, &curr_time);
            printf("\t -> AFTER twoOpt_refinement cost %10.2lf, time %8.2lf",
                   temp_solution_cost,
                   curr_time);
        }

        if (inst->zbest < 0 || temp_solution_cost < inst->zbest) {
            if (is_solution_valid(inst, temp_solution)) {
                inst->zbest = temp_solution_cost;
                inst->tbest = curr_time;
                memcpy(inst->solution, temp_solution, sizeof(unsigned int) * inst->nnodes);
                printf(" (INCUMBENT)");
            } else {
                printf("\n ------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------");
            }
        }
        printf("\n\n");
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

    return
            exitStatus;
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

    //NOTE: The last 2 are already positioned
    for (unsigned int i = 0; i < nodes - 2; i++) {
        for (unsigned int j = i + 1; j < nodes; j++) {
            double tmpDist = get_precalculated_distance(inst, sol[i], sol[j]); // memoized

            //GRASP HERE!
            tmpDist *= 1 + GRASP_alpha * rand_0_1();

            if (minDist == -1 || tmpDist < minDist) {
                minDist = tmpDist;
                pointIndex = j;
            }
        }
        currSol += minDist;
        swap(&sol[i + 1], &sol[pointIndex], sizeof(int));
        minDist = -1.0;
        pointIndex = -1;
    }

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
    printf("\t\t## STARTING twoOpt_refinement\n");

    short exitStatus = 0;

    double initial_cost = calculate_solution_cost(inst, sol);

    while (1) {
        unsigned int *temp_solution = initialize_identity_solution(inst->nnodes);
        memcpy(temp_solution, sol, sizeof(unsigned int) * inst->nnodes);
        double temp_solution_cost_delta = twoOpt_refinement_iteration(inst, temp_solution);
        initial_cost += temp_solution_cost_delta;

        double curr_time = second() - inst->tstart;
        printf("\t\t -> STEP made with new cost %20lf at time %20lf\n", initial_cost, curr_time);
        if (temp_solution_cost_delta == 0)
            return 0;

        *zbest += temp_solution_cost_delta;
        *tbest = curr_time;
        memcpy(sol, temp_solution, sizeof(unsigned int) * inst->nnodes);
        free(temp_solution);

        if (time_limit != 0 && curr_time > time_limit) {
            exitStatus = 1;
            break;
        }
        plot_solution(inst, sol, 0, 0);
        // #ifdef _WIN32
        //         Sleep(100);
        // #else
        //         sleep(0.1);
        // #endif
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

            if (deltaCost < minDeltaCost) {
                minDeltaCost = deltaCost;
                a = i;
                b = j;
            }
        }
    }

    if (minDeltaCost != 0) {
        //MOVE
        swap(&sol[a + 1], &sol[b], sizeof(int));
        reverse_subarray_circular(sol, nodes, a + 2, b);
    }

    return minDeltaCost;
}
