#include "./../headers/utils.h"

#include "../headers/tsp.h"
#include "./../headers/consts.h"

//region UTILITY
void reverse_subcircular(unsigned int *C, unsigned int N, unsigned int l, unsigned int r) {
    //r escluso
    int len = (N - l) + r;
    if (l <= r) len = r - l;

    for (int i = 0; i < len / 2; i++) {
        int left = (l + i) % N;
        int right = (l + len - 1 - i) % N;

        int temp = C[left];
        C[left] = C[right];
        C[right] = temp;
    }
}

/**
 * @brief Prints an error message and terminates the program.
 * 
 * @param err The error message to be printed.
 */
void print_error(const char *err) {
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
 * @brief Initializes the distance matrix for the instance.
 * 
 * @param inst Pointer to the instance structure.
 */
void init_distances(instance *inst) {
    inst->distances = malloc(inst->nnodes * inst->nnodes * sizeof(double));
    for (int i = 0; i < inst->nnodes; i++) {
        for (int j = i + 1; j < inst->nnodes; j++) {
            setDistance(i, j, inst, dist(i, j, inst));
        }
    }
}

/**
 * @brief Shuffles an array of unsigned integers randomly using the Fisher-Yates algorithm.
 * 
 * @param array The array to be shuffled.
 * @param N The number of elements in the array.
 * @return short Returns 0 on success, -1 if memory allocation fails.
 */
short random_shuffle(unsigned int *array, const unsigned int N) {
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
unsigned int *initialize_solution(const unsigned int N) {
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
    // Alloca un buffer temporaneo della dimensione corretta
    void *tmp = malloc(size);

    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);

    free(tmp);
}

//endregion UTILITY

//region MEMORY MANAGEMENT
/**
 * @brief Frees the memory allocated for the instance.
 * 
 * @param inst Pointer to the instance structure to be freed.
 */
void free_instance(instance *inst) {
    // free(inst->demand);
    free(inst->xcoord);
    free(inst->ycoord);
    free(inst->distances);
    // free(inst->load_min);
    // free(inst->load_max);
}

//endregion MEMORY MANAGEMENT

//region PARSER
/**
 * @brief Implementation of the command line parser.
 * 
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @param inst Pointer to the instance structure to be populated.
 */
void parse_command_line(int argc, char **argv, instance *inst) {
    if (VERBOSE >= 100) printf(" running %s with %d parameters \n", argv[0], argc - 1);

    // default
    inst->model_type = 0;
    inst->old_benders = 0;
    strcpy(inst->input_file, "NULL");
    inst->randomseed = 0;
    inst->num_threads = 0;
    inst->timelimit = CPX_INFBOUND; //USARE QUESTI
    inst->cutoff = CPX_INFBOUND;
    //inst->timelimit = 1e10;
    //inst->cutoff = 1e10;
    inst->integer_costs = 0;

    inst->available_memory = 12000; // available memory, in MB, for Cplex execution (e.g., 12000)
    inst->max_nodes = -1; // max n. of branching nodes in the final run (-1 unlimited)

    int help = 0;
    if (argc < 1) help = 1;
    //@COMPLEX@: Command line parser uses a simple loop to match flags and store values in the instance structure.
    for (int i = 1; i < argc; i++) {
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
        if (strcmp(argv[i], "-old_benders") == 0) {
            inst->old_benders = atoi(argv[++i]);
            continue;
        } // old benders
        if (strcmp(argv[i], "-threads") == 0) {
            inst->num_threads = atoi(argv[++i]);
            continue;
        } // n. threads
        if (strcmp(argv[i], "-memory") == 0) {
            inst->available_memory = atoi(argv[++i]);
            continue;
        } // available memory (in MB)
        if (strcmp(argv[i], "-node_file") == 0) {
            strcpy(inst->node_file, argv[++i]);
            continue;
        } // cplex's node file
        if (strcmp(argv[i], "-max_nodes") == 0) {
            inst->max_nodes = atoi(argv[++i]);
            continue;
        } // max n. of nodes
        if (strcmp(argv[i], "-cutoff") == 0) {
            inst->cutoff = atof(argv[++i]);
            continue;
        } // master cutoff
        if (strcmp(argv[i], "-int") == 0) {
            inst->integer_costs = 1;
            continue;
        } // inteher costs
        if ((strcmp(argv[i], "-help") == 0) || strcmp(argv[i], "--help") == 0) {
            help = 1;
            continue;
        } // help
        help = 1;
    }

    if (help || (VERBOSE >= 10)) // print current parameters
    {
        printf("\n\navailable parameters (vers. 16-may-2015) --------------------------------------------------\n");
        printf("-file %s\n", inst->input_file);
        printf("-time_limit %lf\n", inst->timelimit);
        printf("-model_type %d\n", inst->model_type);
        printf("-old_benders %d\n", inst->old_benders);
        printf("-seed %d\n", inst->randomseed);
        printf("-threads %d\n", inst->num_threads);
        printf("-max_nodes %d\n", inst->max_nodes);
        printf("-memory %d\n", inst->available_memory);
        printf("-int %d\n", inst->integer_costs);
        printf("-node_file %s\n", inst->node_file);
        printf("-cutoff %lf\n", inst->cutoff);
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
double dist(unsigned int i, unsigned int j, const instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j];
    if (!inst->integer_costs) return sqrt(dx * dx + dy * dy);
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
double distSquared(unsigned int i, unsigned int j, const instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j];
    if (!inst->integer_costs) return (dx * dx + dy * dy);
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
void read_input(instance *inst) {
    //TODO: Implement here the possibility of generating the random instance
    FILE *fin = fopen(inst->input_file, "r");
    if (fin == NULL) {
        print_error(" input file not found!");
    }

    inst->nnodes = -1;
    inst->depot = -1;
    inst->nveh = -1;

    char line[180];
    char *par_name;
    char *token1;
    char *token2;

    int active_section = 0; // =1 NODE_COORD_SECTION, =0 INFO

    int do_print = (VERBOSE >= 1000);

    //@COMPLEX@: The input reader is a simple TSPLIB parser that handles limited keywords and coordinates.
    while (fgets(line, sizeof(line), fin) != NULL) {
        if (VERBOSE >= 2000) {
            printf("%s", line);
            fflush(NULL);
        }
        if (strlen(line) <= 1) continue; // skip empty lines
        par_name = strtok(line, " :");
        if (VERBOSE >= 3000) {
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
            if (VERBOSE >= 10) printf(" ... solving instance %s with model %d\n\n", token1, inst->model_type);
            continue;
        }

        if (strncmp(par_name, "TYPE", 4) == 0) {
            token1 = strtok(NULL, " :");
            if (strncmp(token1, "TSP", 3) != 0)
                print_error(
                    " format error:  only TYPE == TSP implemented so far!");
            active_section = 0;
            continue;
        }


        if (strncmp(par_name, "DIMENSION", 9) == 0) {
            if (inst->nnodes >= 0) print_error(" repeated DIMENSION section in input file");
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
            if (strncmp(token1, "ATT", 3) != 0)
                print_error(
                    " format error:  only EDGE_WEIGHT_TYPE == ATT implemented so far!");
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "NODE_COORD_SECTION", 18) == 0) {
            if (inst->nnodes <= 0)
                print_error(" ... DIMENSION section should appear before NODE_COORD_SECTION section");
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
            if (i < 0 || i >= inst->nnodes) print_error(" ... unknown node in NODE_COORD_SECTION section");
            token1 = strtok(NULL, " :,");
            token2 = strtok(NULL, " :,");
            inst->xcoord[i] = atof(token1);
            inst->ycoord[i] = atof(token2);
            if (do_print)
                printf(" ... node %4d at coordinates ( %15.7lf , %15.7lf )\n", i + 1, inst->xcoord[i],
                       inst->ycoord[i]);
            continue;
        }

        printf(" final active section %d\n", active_section);
        print_error(" ... wrong format for the current simplified parser!!!!!!!!!");
    }


    fclose(fin);
}

//endregion INPUT READER
