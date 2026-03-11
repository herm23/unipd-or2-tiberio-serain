#include <stdio.h>
#include "./headers/consts.h"
#include "./headers/tsp.h"
#include "./headers/chrono.h"
#include <cplex.h>

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

    double t1 = second();
    instance inst;

    parse_command_line(argc, argv, &inst);
    read_input(&inst);

    unsigned int *solution = initialize_solution(inst.nnodes);

    // printf("Initialized identity solution is ");
    // if (check_tour(&inst, solution) != 0) {
    //     printf("NOT ");
    // }
    // printf("VALID\n");

    const short permutation_status = random_shuffle(solution, inst.nnodes);
    //@COMPLEX@: Random shuffle is used to generate an initial solution for testing purposes.
    if (permutation_status != 0) {
        free(solution);
        free_instance(&inst);
        print_error("Error while calculating random permutation!");
        return 1;
    }

    short check_solution = check_tour(&inst, solution);
    if (check_solution != 0) {
        free(solution);
        free_instance(&inst);
        print_error("Solution after random shuffle is NOT VALID!");
        return 1;
    }

    plot_tour(&inst, solution, 1);
    plot_tour(&inst, solution, 0);

    TSPopt(&inst, solution);

    plot_tour(&inst, solution, 0);

    check_solution = check_tour(&inst, solution);
    if (check_solution != 0) {
        free(solution);
        free_instance(&inst);
        print_error("Solution OPT is NOT VALID!");
        return 1;
    }


    if (VERBOSE >= 1000) {
        for (unsigned int i = 0; i < inst.nnodes; ++i) {
            printf("%d --> %d\n", i, solution[i]);
        }
    }


    double t2 = second();

    if (VERBOSE >= 1) {
        printf("... TSP problem solved in %lf sec.s\n", t2 - t1);
    }

    free(solution);
    free_instance(&inst);
    return 0;
}