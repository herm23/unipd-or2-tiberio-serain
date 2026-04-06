#include <stdio.h>
#include "./headers/consts.h"
#include "./headers/tsp.h"
#include "./headers/chrono.h"
#include <string.h>
#include <stdlib.h>
#include <cplex.h>


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

    if (strcmp(inst.out_file, "NULL") != 0) {
        FILE *f = fopen(inst.out_file, "w");
        if (f == NULL) {
            perror("Error opening output file");
        } else {
            fprintf(f, "Instance: %s\n", inst.input_file);
            fprintf(f, "Nodes: %d\n", inst.nnodes);
            fprintf(f, "Best Cost (zbest): %lf\n", inst.zbest);
            fprintf(f, "Best Time (tbest): %lf\n", inst.tbest);
            fprintf(f, "Solution:\n");
            for (int i = 0; i < inst.nnodes; i++) {
                fprintf(f, "%u ", inst.solution[i]);
            }
            fprintf(f, "\n");
            fclose(f);
        }
    }

    free_instance(&inst);
    return 0;
}


/**
 * @brief Tenta di aggiornare la soluzione migliore dell'istanza con una nuova proposta.
 */
void update_instance_solution(instance *inst, unsigned int *new_sol, double new_cost, double new_time) {
    if (inst->zbest < 0 || new_cost < inst->zbest) {
        if (is_solution_valid(inst, new_sol)) {
            inst->zbest = new_cost;
            inst->tbest = new_time;
            memcpy(inst->solution, new_sol, sizeof(unsigned int) * inst->nnodes);
        } else {
            printf("------------ FOUND AN INVALID OPTIMAL SOLUTION!!! ------------\n");
        }
    }
}

/**
 * @brief Execute the refinement VNS: 2-opt -> Kick -> 2-opt.
 */
void apply_vns_refinement(instance *inst, double timelimit, unsigned int *sol, double *cost, double *time) {
    twoOpt_refinement(inst, timelimit, sol, cost, time);
    vns_kick(inst, timelimit, sol, cost, time);

    //Debug
    //plot_solution(inst, sol, 0, 1);

    twoOpt_refinement(inst, timelimit, sol, cost, time);
}

/**
 * @brief Prepara una soluzione temporanea partendo da quella attuale dell'istanza.
 */
unsigned int *prepare_temp_solution(instance *inst, double *cost, double *time) {
    unsigned int *temp = initialize_identity_solution(inst->nnodes);
    memcpy(temp, inst->solution, sizeof(unsigned int) * inst->nnodes);
    *cost = inst->zbest;
    *time = inst->tbest;
    return temp;
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

    // enum TSP_SOLVING_STRATEGY strategy = GRASP_TWO_OPT_MULTISTART;
    enum TSP_SOLVING_STRATEGY strategy = NN_AND_VNS;

    //NOTE: Managing hyperparameters without code duplication
    //Hard-code for now
    switch (strategy) {
        case NN:
        case NN_TWO_OPT_MULTISTART:
        case NN_AND_TWO_OPT: {
            inst->GRASP_alpha = 0;
            inst->VNS_kick_counter = 0;
        }
        break;
        case NN_AND_VNS: {
            inst->GRASP_alpha = 0;
            inst->VNS_kick_counter = 3;
        }
        break;
        case GRASP:
        case GRASP_TWO_OPT_MULTISTART:
        case GRASP_AND_TWO_OPT: {
            inst->GRASP_alpha = 0.1;
            inst->VNS_kick_counter = 0;
        }
        break;
    }

    switch (strategy) {
        case NN:
        case GRASP: // In questo caso GRASP usa il comportamento base (NONE)
            greedy_nn_heuristic(inst, inst->timelimit, NONE);
            break;

        case NN_TWO_OPT_MULTISTART:
        case GRASP_TWO_OPT_MULTISTART:
            greedy_nn_heuristic(inst, inst->timelimit, TWO_OPT);
            break;

        case NN_VNS_MULTISTART:
        case GRASP_VNS_MULTISTART:
            greedy_nn_heuristic(inst, inst->timelimit, VNS);
            break;

        case NN_AND_TWO_OPT:
        case GRASP_AND_TWO_OPT: {
            double ratio = (strategy == GRASP_AND_TWO_OPT) ? 0.2 : 1.0;
            greedy_nn_heuristic(inst, inst->timelimit * ratio, NONE);

            temp_solution = prepare_temp_solution(inst, &temp_solution_cost, &curr_time);

            twoOpt_refinement(inst, inst->timelimit * (1.0 - ratio + 0.1), temp_solution, &temp_solution_cost,
                              &curr_time);

            update_instance_solution(inst, temp_solution, temp_solution_cost, curr_time);
            free(temp_solution);
        }
        break;

        case NN_AND_VNS:
        case GRASP_AND_VNS: {
            double ratio = (strategy == GRASP_AND_VNS) ? 0.2 : 1.0;
            greedy_nn_heuristic(inst, inst->timelimit * ratio, NONE);

            temp_solution = prepare_temp_solution(inst, &temp_solution_cost, &curr_time);

            apply_vns_refinement(inst, inst->timelimit * (1.0 - ratio + 0.1), temp_solution, &temp_solution_cost,
                                 &curr_time);

            update_instance_solution(inst, temp_solution, temp_solution_cost, curr_time);
            free(temp_solution);
        }
        break;

        default:
            printf("ERROR: Unknown solving strategy\n");
            break;
    }

    plot_solution(inst, inst->solution, 0, 1);
    return 0;
}
