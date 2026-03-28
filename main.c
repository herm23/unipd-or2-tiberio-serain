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
    if (check_solution != 0) {
        free_instance(&inst);
        print_error("Solution found is NOT VALID!");
        return 1;
    }

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

    printf("## COST %15lf AT TIME 0\n", solution_cost);

    plot_solution(inst, inst->solution, 0, 0);

    greedy_nn_heuristic(inst, inst->timelimit, TWO_OPT);
    plot_solution(inst, inst->solution, 0, 1);

    // twoOpt_refinement(inst, inst->solution);
    // plot_solution(inst, inst->solution, 0, 0);

    return 0;
}
