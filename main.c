#include <stdio.h>
#include "./headers/consts.h"
#include "./headers/tsp.h"
#include "./headers/chrono.h"
#include <cplex.h>
#include <string.h>


short TSP_OPT(instance *inst);

/**
 * @brief The main entry point of the program.
 * 
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @return int Returns 0 on success, 1 on error.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s -help for help\n", argv[0]);
        exit(1);
    }
    if (VERBOSE >= 2) {
        for (int a = 0; a < argc; a++) printf("%s ", argv[a]);
        printf("\n");
    }

    instance inst;


    const double t1 = second();
    parse_command_line(argc, argv, &inst);
    read_input(&inst);
    init_distances(&inst);


    TSP_OPT(&inst);

    short check_solution = is_solution_valid(&inst, inst.solution);
    if (!check_solution) {
        free_instance(&inst);
        print_error_and_die("Solution found is NOT VALID!");
        return 1;
    }

    printf("## COST %15lf AT TIME %lf\n", inst.zbest, inst.tbest);


    if (VERBOSE >= 1000) {
        printf("FINAL SOLUTION:\n");
        for (unsigned int i = 0; i < inst.nnodes; ++i) {
            printf("%d --> %d\n", i, inst.solution[i]);
        }
    }

    const double t2 = second();

    if (VERBOSE >= 1) {
        printf("Program terminated in %lf sec.s\n", t2 - t1);
    }


    free_instance(&inst);
    return 0;
}


/**
 * @brief Optimizes the TSP instance using the chosen heuristic.
 * 
 * @param inst Pointer to the instance structure.
 * @return short Returns 0 on success.
 */
short TSP_OPT(instance *inst) {
    // INIT
    inst->solution = initialize_identity_solution(inst->nnodes);

    plot_solution(inst, inst->solution, 1, 1);
    plot_solution(inst, inst->solution, 0, 1);


    const double starting_time = second();

    const double solution_cost = calculate_solution_cost(inst, inst->solution);
    inst->tbest = starting_time;
    inst->tstart = starting_time;

    inst->zbest = solution_cost;
    inst->zstart = solution_cost;

    printf("\n\n## FINAL OPTIMAL / INCUMBEMT SOLUTION WITH COST %15lf FOUND AT TIME 0\n", solution_cost);

    unsigned int *temp_solution = NULL;
    double temp_solution_cost;
    double curr_time;

    enum TSP_SOLVING_STRATEGY strategy = GRASP_TWO_OPT_MULTISTART;

    switch (strategy) {
        case NN:
            greedy_nn_heuristic(inst, inst->timelimit, 0, NONE);
            break;
        case NN_TWO_OPT_MULTISTART:
            greedy_nn_heuristic(inst, inst->timelimit, 0, TWO_OPT);
            break;
        case NN_AND_TWO_OPT:
            greedy_nn_heuristic(inst, inst->timelimit, 0, NONE);

            temp_solution = initialize_identity_solution(inst->nnodes);
            memcpy(temp_solution, inst->solution, sizeof(unsigned int) * inst->nnodes);
            temp_solution_cost = inst->zbest;
            curr_time = inst->tbest;

            twoOpt_refinement(inst, inst->timelimit, temp_solution, &temp_solution_cost, &curr_time);

            if (inst->zbest < 0 || temp_solution_cost < inst->zbest) {
                if (is_solution_valid(inst, temp_solution)) {
                    inst->zbest = temp_solution_cost;
                    inst->tbest = curr_time;
                    memcpy(inst->solution, temp_solution, sizeof(unsigned int) * inst->nnodes);
                } else {
                    printf("------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------");
                }
            }

            break;
        case GRASP:
            greedy_nn_heuristic(inst, inst->timelimit, 0.1, NONE);
            break;
        case GRASP_TWO_OPT_MULTISTART:
            greedy_nn_heuristic(inst, inst->timelimit, 0.1, TWO_OPT);
            break;
        case GRASP_AND_TWO_OPT:
            printf("Running GRASP and 2-opt refinement with 20%% (%lf) time limit for GRASP and 80%% (%lf) for 2-opt refinement\n", inst->timelimit * 0.2, inst->timelimit * 0.8);
            greedy_nn_heuristic(inst, inst->timelimit * 0.2, 0.1, NONE);

            temp_solution = initialize_identity_solution(inst->nnodes);
            memcpy(temp_solution, inst->solution, sizeof(unsigned int) * inst->nnodes);
            temp_solution_cost = inst->zbest;
            curr_time = inst->tbest;

            twoOpt_refinement(inst, inst->timelimit * 0.8, temp_solution, &temp_solution_cost, &curr_time);

            if (inst->zbest < 0 || temp_solution_cost < inst->zbest) {
                if (is_solution_valid(inst, temp_solution)) {
                    inst->zbest = temp_solution_cost;
                    inst->tbest = curr_time;
                    memcpy(inst->solution, temp_solution, sizeof(unsigned int) * inst->nnodes);
                } else {
                    printf("------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------");
                }
            }
            break;

        default:
            printf("ERROR: Unknown solving strategy\n");
    }
    plot_solution(inst, inst->solution, 0, 1);
    return 0;
}
