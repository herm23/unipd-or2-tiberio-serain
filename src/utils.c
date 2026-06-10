#include "./../headers/utils.h"

#include "../headers/tsp.h"
#include "./../headers/consts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

//region UTILITY
/**
 * @brief Reverses a subarray in a circular fashion within an array of unsigned integers.
 * 
 * @param C The array of unsigned integers.
 * @param N The size of the array.
 * @param l The starting index of the subarray to be reversed.
 * @param r The ending index of the subarray to be reversed.
 */
void reverse_subarray_circular(unsigned int *C, unsigned int N, unsigned int l, unsigned int r) {
    //r escluso
    unsigned int len = (N - l) + r;
    if (l <= r) len = r - l;

    for (int i = 0; i < len / 2; i++) {
        unsigned int left = (l + i) % N;
        unsigned int right = (l + len - 1 - i) % N;

        unsigned int temp = C[left];
        C[left] = C[right];
        C[right] = temp;
    }
}

/**
 * @brief Prints an error message to stderr and terminates the program.
 * 
 * @param err The error message to be printed.
 */
void print_error_and_die(const char *err) {
    printf("\n\n ERROR: %s \n\n", err);
    fflush(NULL);
    exit(1);
}

/**
 * @brief Prints the elements of an integer array.
 * 
 * @param array The array to be printed.
 * @param size The number of elements in the array.
 */
void print_array(int *array, int size) {
    printf("[");
    for (int i = 0; i < size; i++) {
        printf("%d", array[i]);

        // Aggiungi una virgola tra gli elementi, ma non dopo l'ultimo
        if (i < size - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

/**
 * @brief Initializes the distance matrix for the instance by calculating all pairwise distances.
 * 
 * @param inst Pointer to the instance structure.
 */
void init_distances(instance *inst) {
    // M1 fix: only the upper triangle is ever indexed (max index n(n-1)/2 - 1), so allocate
    // n(n-1)/2 doubles instead of n^2 (~2x saving), using size_t to avoid int overflow for large n.
    // M4: guard the allocation result.
    size_t n = (size_t) inst->nnodes;
    size_t count = n * (n - 1) / 2;
    inst->distances = malloc(count * sizeof(double));
    if (count > 0 && inst->distances == NULL) print_error_and_die("init_distances: out of memory");
    for (int i = 0; i < inst->nnodes; i++) {
        for (int j = i + 1; j < inst->nnodes; j++) {
            set_precalculated_distance(inst, i, j, calculate_distance(i, j, inst));
        }
    }
}

/**
 * @brief Shuffles an array of unsigned integers randomly.
 * 
 * @param array The array to be shuffled.
 * @param N The number of elements in the array.
 * @return short Returns 0 on success.
 */
short random_array_shuffle(unsigned int *array, const unsigned int N) {
    //Return 0 = ok
    if (N <= 1) return -1;

    for (unsigned int i = N - 1; i > 0; --i) {
        //@COMPLEX@: Fisher-Yates shuffle algorithm: swaps the current element with a random one from the remaining pool.
        size_t j = rand() % (i + 1); //Sarebbe da usare una cosa più robusta di rand, ma per questo prototipo ok
        swap(&array[i], &array[j], sizeof(int));
    }

    return 0;
}

/**
 * @brief Initializes a solution array with an identity mapping (0, 1, 2, ..., N-1).
 * 
 * @param N The number of nodes in the solution.
 * @return unsigned int* Pointer to the newly allocated and initialized solution array.
 */
unsigned int *initialize_identity_solution(const unsigned int N) {
    //Keeping all unsigned (all indexes are positive)
    unsigned int *solution = malloc(N * sizeof(unsigned int));
    if (!solution) return NULL;
    for (unsigned int i = 0; i < N; ++i) {
        solution[i] = i;
    }
    return solution;
}

/**
 * @brief Generic swap function for two elements of a given size.
 * 
 * @param a Pointer to the first element.
 * @param b Pointer to the second element.
 * @param size Size of the elements to be swapped.
 */
void swap(void *a, void *b, size_t size) {
    void *tmp = malloc(size);

    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);

    free(tmp);
}

/**
 * @brief Generates a random floating-point number between 0 and 1.
 * 
 * @return double A random number in the range [0, 1].
 */
double rand_0_1() {
    return (double) rand() / (double) RAND_MAX;
}

//endregion UTILITY

//region MEMORY MANAGEMENT
/**
 * @brief Frees the memory allocated for the instance structure and its components.
 * 
 * @param inst Pointer to the instance structure to be freed.
 */
void free_instance(instance *inst) {
    // M5 fix: null out pointers after freeing so an accidental second free_instance() is a no-op.
    free(inst->xcoord);    inst->xcoord = NULL;
    free(inst->ycoord);    inst->ycoord = NULL;
    free(inst->distances); inst->distances = NULL;
    free(inst->solution);  inst->solution = NULL;
    free(inst->tabulist);  inst->tabulist = NULL;
    if (inst->plot) {
        pclose(inst->plot);
        inst->plot = NULL;
    }
    if (inst->cost_plot) {
        fclose(inst->cost_plot);
        inst->cost_plot = NULL;
    }
}

//endregion MEMORY MANAGEMENT

//region PARSER

/**
 * @brief Displays the program header including the current system date.
 *
 * Generates a versioning string based on the execution date (format: DD-MMM-YYYY) 
 * and prints it as part of the "available parameters" section of the output.
 */
void print_header() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date_str[32];

    // Array di mesi abbreviati per corrispondere al tuo formato
    const char *months[] = {
        "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };

    // Formattiamo la data: %02d forza lo zero iniziale per i giorni < 10
    sprintf(date_str, "%02d-%s-%d", tm->tm_mday, months[tm->tm_mon], tm->tm_year + 1900);

    printf("available parameters (vers. %s) --------------------------------------------------\n", date_str);
}

/**
 * @brief Prints the list of available TSP solving strategies with their corresponding numeric indices.
 *
 * This function iterates through the TSP_SOLVING_STRATEGY enumeration and
 * displays each strategy with its numeric index using a switch statement for mapping.
 */
void print_tsp_strategies() {
    printf("Strategie disponibili:\n  ");

    // Usiamo CPLEX come limite superiore del ciclo essendo l'ultimo elemento
    for (int i = 0; i <= CPLEX; i++) {
        char *name;
        switch ((enum TSP_SOLVING_STRATEGY) i) {
            case NN: name = "NN";
                break;
            case NN_TWO_OPT_MULTISTART: name = "NN_2OPT_MS";
                break;
            case NN_AND_TWO_OPT: name = "NN_2OPT";
                break;
            case NN_AND_VNS: name = "NN_VNS";
                break;
            case NN_AND_TABUSEARCH: name = "NN_TABU";
                break;
            case GRASP: name = "GRASP";
                break;
            case GRASP_TWO_OPT_MULTISTART: name = "GRASP_2OPT_MS";
                break;
            case GRASP_AND_TWO_OPT: name = "GRASP_2OPT";
                break;
            case GRASP_AND_VNS: name = "GRASP_VNS";
                break;
            case GRASP_AND_TABUSEARCH: name = "GRASP_TABU";
                break;
            case EXTRA_MILEAGE: name = "EXTRA_MILEAGE";
                break;
            case CPLEX: name = "CPLEX";
                break;
            default: name = "UNKNOWN";
                break;
        }

        printf("%d: %s%s", i, name, (i == CPLEX ? "" : ", "));
    }
    printf("\n");
}

/**
 * @brief Implementation of the command line parser.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @param inst Pointer to the instance structure to be populated.
 */
void parse_command_line(int argc, char **argv, instance *inst) {
    // default
    inst->zbest = -1;
    inst->nnodes = 0; // C9 fix: explicit default so file-input runs don't see garbage as "random params"
    inst->model_type = 0;
    strcpy(inst->input_file, "NULL");
    strcpy(inst->out_file, "NULL");
    inst->randomseed = -1;
    inst->timelimit = 1.0E+20;
    inst->no_plot = 0;
    inst->verbosity = DEFAULT_VERBOSITY;
    inst->cplex_mode = CALLBACKS; // default preserves previous hardcoded behavior

    int help = 0;
    if (argc < 1) help = 1;
    //@COMPLEX@: Command line parser uses a simple loop to match flags and store values in the instance structure.
    for (int i = 1; i < argc; i++) {
        //region VERBOSITY
        if (
            (strcmp(argv[i], "-verbosity") == 0) ||
            (strcmp(argv[i], "-v") == 0)
        ) {
            inst->verbosity = atoi(argv[++i]);
            continue;
        }
        //endregion VERBOSITY
        //region INPUT FILE
        if (
            (strcmp(argv[i], "-file") == 0) ||
            (strcmp(argv[i], "-input") == 0) ||
            (strcmp(argv[i], "-f") == 0)
        ) {
            strcpy(inst->input_file, argv[++i]);
            continue;
        }
        //endregion INPUT FILE

        //region OUTPUT FILE
        if (
            (strcmp(argv[i], "--out") == 0) ||
            (strcmp(argv[i], "-out") == 0)
        ) {
            strcpy(inst->out_file, argv[++i]);
            continue;
        }
        //endregion OUTPUT FILE

        //region NO PLOT
        if (
            (strcmp(argv[i], "--no-plot") == 0) ||
            (strcmp(argv[i], "-no-plot") == 0)
        ) {
            inst->no_plot = 1;
            continue;
        }
        //endregion NO PLOT

        //region RANDOM INSTANCE
        if (strcmp(argv[i], "-seed") == 0) {
            inst->randomseed = abs(atoi(argv[++i]));
            continue;
        } // random seed

        if (strcmp(argv[i], "-nodes_count") == 0) {
            inst->nnodes = abs(atoi(argv[++i]));
            continue;
        }
        //endregion RANDOM INSTANCE

        if (strcmp(argv[i], "-time_limit") == 0) {
            inst->timelimit = atof(argv[++i]);
            continue;
        } // total time limit
        if ((strcmp(argv[i], "-model_type") == 0) ||
            (strcmp(argv[i], "-model") == 0)
        ) {
            inst->model_type = atoi(argv[++i]);
            continue;
        } // model type
        if ((strcmp(argv[i], "-cplex_mode") == 0) ||
            (strcmp(argv[i], "-cplex-mode") == 0)
        ) {
            const char *m = argv[++i];
            if (strcmp(m, "benders") == 0) inst->cplex_mode = BENDERS_LOOP;
            else if (strcmp(m, "callbacks") == 0) inst->cplex_mode = CALLBACKS;
            else if (strcmp(m, "hardfixing") == 0) inst->cplex_mode = HARD_FIXING;
            else if (strcmp(m, "localbranching") == 0) inst->cplex_mode = LOCAL_BRANCHING;
            else print_error_and_die("Unknown -cplex_mode (use benders|callbacks|hardfixing|localbranching)");
            continue;
        } // CPLEX mode selection (only relevant when -model points to CPLEX)
        if ((strcmp(argv[i], "-help") == 0) || strcmp(argv[i], "--help") == 0) {
            help = 1;
            continue;
        } // help
        //help = 1;
    }

    if (inst->verbosity >= DEBUG) printf(" running %s with %d parameters \n", argv[0], argc - 1);

    if (help || (inst->verbosity >= INFO)) // print current parameters
    {
        print_header();
        printf("-file %s\n", inst->input_file);
        printf("-time_limit %lf\n", inst->timelimit);
        printf("-model_type %d\n", inst->model_type);
        printf("-cplex_mode %d (0=benders, 1=callbacks, 2=hardfixing, 3=localbranching)\n", inst->cplex_mode);
        print_tsp_strategies();
        printf("-seed %d\n", inst->randomseed);
        printf("-verbosity %d\n", inst->verbosity);
        printf("\nenter -help or --help for help\n");
        printf("----------------------------------------------------------------------------------------------\n\n");
    }

    if (help) exit(1);
}

//endregion PARSER

//region DISTANCE CALCULATION
/**
 * @brief Calculates the distance between two nodes based on the instance's edge weight type.
 *
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated distance between node i and node j.
 */
double calculate_distance(unsigned int i, unsigned int j, const instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j];
    // if (!inst->integer_costs) return sqrt(dx * dx + dy * dy);
    int dis = sqrt(dx * dx + dy * dy) + 0.499999999; // nearest integer
    return dis + 0.0;
}

/**
 * @brief Calculates the squared distance between two nodes.
 *
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated squared distance between node i and node j.
 */
double calculate_squared_distance(unsigned int i, unsigned int j, const instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j];
    // if (!inst->integer_costs) return (dx * dx + dy * dy);
    int dis = (dx * dx + dy * dy) + 0.499999999; // nearest integer
    return dis + 0.0;
}

//endregion DISTANCE CALCULATION

//region INPUT READER
/**
 * @brief Implementation of the input reader.
 *
 * @param inst Pointer to the instance structure to be populated.
 */

//WARNING: Does it makes sense to print_error and exit program ?
/**
 * @brief Parses a TSP file in TSPLIB format and populates the instance structure.
 *
 * @param inst Pointer to the instance structure.
 */
void parse_tsp_file(instance *inst) {
    FILE *fin = fopen(inst->input_file, "r");
    if (fin == NULL) {
        print_error_and_die(" input file not found!");
    }

    inst->nnodes = -1;
    inst->plot = NULL;
    inst->cost_plot = NULL;

    char line[180];
    char *par_name;
    char *token1;
    char *token2;

    int active_section = 0; // =1 NODE_COORD_SECTION, =0 INFO

    int do_print = (inst->verbosity >= VERBOSE_LEVEL);

    //@COMPLEX@: The input reader is a simple TSPLIB parser that handles limited keywords and coordinates.
    while (fgets(line, sizeof(line), fin) != NULL) {
        if (inst->verbosity >= TRACE) {
            printf("%s", line);
            fflush(NULL);
        }
        if (strlen(line) <= 1) continue; // skip empty lines
        par_name = strtok(line, " :");
        if (inst->verbosity >= TRACE) {
            printf("parameter \"%s\" ", par_name);
            fflush(NULL);
        }

        if (strncmp(par_name, "NAME", 4) == 0) {
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "COMMENT", 7) == 0) {
            active_section = 0;
            token1 = strtok(NULL, "");
            if (inst->verbosity >= INFO) printf(" ... solving instance %s with model %d\n\n", token1, inst->model_type);
            continue;
        }

        if (strncmp(par_name, "TYPE", 4) == 0) {
            token1 = strtok(NULL, " :");
            if (strncmp(token1, "TSP", 3) != 0)
                print_error_and_die(
                    " format error:  only TYPE == TSP implemented so far!");
            active_section = 0;
            continue;
        }


        if (strncmp(par_name, "DIMENSION", 9) == 0) {
            if (inst->nnodes >= 0) print_error_and_die(" repeated DIMENSION section in input file");
            token1 = strtok(NULL, " :");
            inst->nnodes = atoi(token1);
            if (do_print) printf(" ... nnodes %d\n", inst->nnodes);
            inst->xcoord = (double *) calloc(inst->nnodes, sizeof(double));
            inst->ycoord = (double *) calloc(inst->nnodes, sizeof(double));
            active_section = 0;
            continue;
        }


        if (strncmp(par_name, "EDGE_WEIGHT_TYPE", 16) == 0) {
            token1 = strtok(NULL, " :");
            // C7 fix: the distance code computes nearest-integer Euclidean (EUC_2D). Accept both EUC_2D
            // (matches the math exactly) and ATT (treated as the same Euclidean approximation) instead of
            // rejecting EUC_2D files outright.
            if (strncmp(token1, "ATT", 3) != 0 && strncmp(token1, "EUC_2D", 6) != 0)
                print_error_and_die(
                    " format error:  only EDGE_WEIGHT_TYPE == ATT or EUC_2D implemented so far!");
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "NODE_COORD_SECTION", 18) == 0) {
            if (inst->nnodes <= 0)
                print_error_and_die(" ... DIMENSION section should appear before NODE_COORD_SECTION section");
            active_section = 1;
            continue;
        }


        if (strncmp(par_name, "EOF", 3) == 0) {
            active_section = 0;
            break;
        }


        if (active_section == 1) // within NODE_COORD_SECTION
        {
            int i = atoi(par_name) - 1;
            //@COMPLEX@: Converts 1-based node index from file to 0-based array index and parses coordinates.
            if (i < 0 || i >= inst->nnodes) print_error_and_die(" ... unknown node in NODE_COORD_SECTION section");
            token1 = strtok(NULL, " :,");
            token2 = strtok(NULL, " :,");
            inst->xcoord[i] = atof(token1);
            inst->ycoord[i] = atof(token2);
            if (do_print)
                printf(" ... node %4d at coordinates ( %15.7lf , %15.7lf )\n", i + 1, inst->xcoord[i],
                       inst->ycoord[i]);
            continue;
        }

        if (inst->verbosity >= TRACE) printf(" final active section %d\n", active_section);
        print_error_and_die(" ... wrong format for the current simplified parser!!!!!!!!!");
    }


    fclose(fin);
}

/**
 * @brief Generates a random TSP instance with a specified number of nodes.
 *
 * @param inst Pointer to the instance structure.
 */
void generate_random_instance(instance *inst) {
    if (inst->nnodes <= 0) {
        print_error_and_die("Invalid number of nodes for random generation");
    }
    // if (inst->randomseed == DEFAULTSEEDVAL) {
    //     print_error_and_die("Invalid random seed passed");
    // }

    inst->xcoord = (double *) calloc(inst->nnodes, sizeof(double));
    inst->ycoord = (double *) calloc(inst->nnodes, sizeof(double));

    if (inst->xcoord == NULL || inst->ycoord == NULL) {
        print_error_and_die("Memory allocation failed in generate_random_instance");
    }

    if (inst->randomseed != DEFAULTSEEDVAL) {
        srand(inst->randomseed);
    } else {
        srand(time(0));
    }

    for (int i = 0; i < inst->nnodes; i++) {
        inst->xcoord[i] = ((double) rand() / (double) RAND_MAX) * XMAX;
        inst->ycoord[i] = ((double) rand() / (double) RAND_MAX) * YMAX;

        if (inst->verbosity >= TRACE) {
            printf("Point %d, has cooordinates (%lf, %lf)\n", i, inst->xcoord[i], inst->ycoord[i]);
        }
    }

    if (inst->verbosity >= INFO) {
        printf(" ... generated random instance with %d nodes and seed %u\n", inst->nnodes, inst->randomseed);
    }
    inst->plot = NULL;
    inst->cost_plot = NULL;
}

/**
 * @brief Decides whether to parse a TSP file or generate a random instance based on arguments.
 * 
 * @param inst Pointer to the instance structure.
 */
void read_input(instance *inst) {
    bool has_input_file = (strcmp(inst->input_file, DEFAULT_INPUT) != 0);
    bool has_random_params = (inst->nnodes > 0);
    //bool has_random_params = (inst->randomseed != DEFAULTSEEDVAL && inst->nnodes > 0);

    if (has_input_file && has_random_params) {
        print_error_and_die("Invalid commands: please pass the input file OR the random instance");
    }

    if (has_input_file) {
        parse_tsp_file(inst);
        return;
    }

    if (has_random_params) {
        generate_random_instance(inst);
        return;
    }

    print_error_and_die("TSP instance not found");
}

//endregion INPUT READER
