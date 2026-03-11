#include "./../headers/tsp.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

/**
 * @brief Plots the tour using gnuplot by generating data and command files.
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array representing the tour.
 * @param only_instance Flag to plot only the instance nodes without the tour edges.
 */
void plot_tour(const instance *inst, const unsigned int *solution, int only_instance) {
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
    FILE *gp = popen("gnuplot -persistent", "w");
    if (gp == NULL) {
        perror("plot_tour error: popen failed");
        return;
    }

    /* ----------- Gnuplot configuration ----------- */
    fprintf(gp, "set title 'TSP Visualization'\n");
    fprintf(gp, "set xlabel 'X'\n");
    fprintf(gp, "set ylabel 'Y'\n");
    fprintf(gp, "set grid\n");
    fprintf(gp, "set key off\n");
    fprintf(gp, "set size ratio -1\n"); /* keep aspect ratio */

    if (only_instance) {
        /* ---- Plot only nodes ---- */
        fprintf(gp, "plot '-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(gp, "%f %f\n", inst->xcoord[i], inst->ycoord[i]);
        }

        fprintf(gp, "e\n");
    } else {
        /* ---- Plot nodes + tour ---- */
        fprintf(gp,
                "plot '-' with lines lw 2 lc rgb 'red', "
                "'-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        /* ---- Tour edges (closed) ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            int idx = solution[i];

            if (idx < 0 || idx >= inst->nnodes) {
                fprintf(stderr, "plot_tour error: invalid index in solution\n");
                pclose(gp);
                return;
            }

            fprintf(gp, "%f %f\n",
                    inst->xcoord[idx],
                    inst->ycoord[idx]);
        }

        /* Close the tour */
        int first = solution[0];
        fprintf(gp, "%f %f\n",
                inst->xcoord[first],
                inst->ycoord[first]);

        fprintf(gp, "e\n");

        /* ---- Nodes ---- */
        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(gp, "%f %f\n",
                    inst->xcoord[i],
                    inst->ycoord[i]);
        }

        fprintf(gp, "e\n");
    }

    fflush(gp);
    pclose(gp);
}


/**
 * @brief Checks if a solution represents a valid Hamiltonian tour (cycle).
 * 
 * @param inst Pointer to the instance structure.
 * @param solution The solution array to be checked.
 * @return short Returns 0 if valid, -1 if invalid or if memory allocation fails.
 */
short check_tour(const instance *inst, const unsigned int *solution) {
    //Return 0 if valid, -1 otherways
    if (inst == NULL || solution == NULL) return -1;

    //NOTE: NO CHECKS ON INSTANCE OR SOLUTION VALIDITY (implicits on previous functions)

    const unsigned int N = inst->nnodes;
    unsigned char *visited = calloc(N, sizeof(unsigned char)); //with calloc we avoid the for loop for zero-init

    if (!visited) return -1;

    for (unsigned int i = 0; i < N; ++i) {
        unsigned int node = solution[i];

        //@COMPLEX@: Validates a Hamiltonian cycle by ensuring each node is visited exactly once using a bit/byte-set.
        if (node >= N || visited[node]) {
            //we can do only a single pass like this  if we know the range of elements
            free(visited);
            return -1;
        }

        visited[node] = 1;
    }

    free(visited);
    return 0;
}


/**
 * @brief Calculates the distance between two nodes based on the instance's edge weight type.
 * 
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @param inst Pointer to the instance structure.
 * @return double The calculated distance between node i and node j.
 */
double dist(unsigned int i, unsigned int j, instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j];
    if (!inst->integer_costs) return sqrt(dx * dx + dy * dy);
    int dis = sqrt(dx * dx + dy * dy) + 0.499999999; // nearest integer
    return dis + 0.0;
}

double distSquared(unsigned int i, unsigned int j, instance *inst) {
    double dx = inst->xcoord[i] - inst->xcoord[j];
    double dy = inst->ycoord[i] - inst->ycoord[j]; 
    if (!inst->integer_costs) return (dx * dx + dy * dy);
    int dis = (dx * dx + dy * dy) + 0.499999999; // nearest integer
    return dis + 0.0;
}

/**
 * @brief Calculates the total cost of a given tour.
 * 
 * @param inst Pointer to the instance structure.
 * @param sol The solution array representing the tour.
 * @return double The total cost of the tour.
 */
static double tour_cost(instance *inst, unsigned int *sol) {
    double cost = 0.0;
    int N = inst->nnodes;

    for (int i = 0; i < N - 1; i++)
        cost += dist(sol[i], sol[i + 1], inst);

    cost += dist(sol[N - 1], sol[0], inst);
    return cost;
}



// Helper per controllo errori CPLEX
/**
 * @brief Helper function to check CPLEX operation status and handle errors.
 * 
 * @param status The status code returned by a CPLEX function.
 * @param env The CPLEX environment pointer.
 * @param where A string describing where the error occurred.
**/
static void cpx_check(int status, CPXENVptr env, const char *where) {
    if (status) {
        char errmsg[1024];
        CPXgeterrorstring(env, status, errmsg);
        fprintf(stderr, "CPLEX error at %s: %s (status=%d)\n", where, errmsg, status);
        exit(1);
    }
}


/**
 * @brief Solves the TSP problem to optimality using CPLEX and the MTZ formulation.
 * 
 * @param inst Pointer to the instance structure.
 * @param sol Pointer to the solution array to be populated with the optimal tour.
 **/
void TSPopt(instance *inst, unsigned int *sol) {
    const int n = inst->nnodes;
    if (n <= 1) {
        if (n == 1) sol[0] = 0;
        inst->zbest = 0.0;
        return;
    }

    int status = 0;
    CPXENVptr env = CPXopenCPLEX(&status);
    if (!env || status) {
        fprintf(stderr, "Failed to open CPLEX (status=%d)\n", status);
        exit(1);
    }

    CPXLPptr lp = CPXcreateprob(env, &status, "TSP_MTZ");
    cpx_check(status, env, "CPXcreateprob");

    // --- Variabili ---
    // x_ij: n*n (con x_ii fissata a 0)
    // u_i:  n   (MTZ)
    const int n_x = n * n;
    const int n_u = n;
    const int total_vars = n_x + n_u;

    double *obj = (double *) calloc(total_vars, sizeof(double));
    double *lb = (double *) calloc(total_vars, sizeof(double));
    double *ub = (double *) calloc(total_vars, sizeof(double));
    char *ctype = (char *) calloc(total_vars, sizeof(char));
    if (!obj || !lb || !ub || !ctype) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    // x_ij
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int k = i * n + j;
            obj[k] = (i == j) ? 0.0 : dist(i, j, inst);
            lb[k] = 0.0;
            ub[k] = (i == j) ? 0.0 : 1.0; // x_ii = 0
            ctype[k] = 'B';
        }
    }

    // u_i (MTZ)
    // u_0 = 0 fissato
    // u_i in [1, n-1] per i>=1
    for (int i = 0; i < n; i++) {
        int k = n_x + i;
        obj[k] = 0.0;
        ctype[k] = 'C';
        if (i == 0) {
            lb[k] = 0.0;
            ub[k] = 0.0;
        } else {
            lb[k] = 1.0;
            ub[k] = (double) (n - 1);
        }
    }

    status = CPXnewcols(env, lp, total_vars, obj, lb, ub, ctype, NULL);
    cpx_check(status, env, "CPXnewcols");

    // --- Vincoli di grado ---
    // 1) Per ogni i: sum_{j != i} x_{i,j} = 1 (uscita)
    for (int i = 0; i < n; i++) {
        int *ind = (int *) malloc(n * sizeof(int));
        double *val = (double *) malloc(n * sizeof(double));
        if (!ind || !val) {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        int cnt = 0;
        for (int j = 0; j < n; j++) {
            if (j == i) continue;
            ind[cnt] = i * n + j;
            val[cnt] = 1.0;
            cnt++;
        }

        double rhs = 1.0;
        char sense = 'E';
        int rmatbeg[1];
        rmatbeg[0] = 0;

        status = CPXaddrows(env, lp,
                            0, // nessuna nuova colonna
                            1, // una riga
                            cnt, // numero di coefficienti
                            &rhs,
                            &sense,
                            rmatbeg, // ← NON NULL
                            ind,
                            val,
                            NULL,
                            NULL);
        cpx_check(status, env, "CPXaddrows(out-degree)");

        free(ind);
        free(val);
    }

    // 2) Per ogni j: sum_{i != j} x_{i,j} = 1 (ingresso)
    for (int j = 0; j < n; j++) {
        int *ind = (int *) malloc(n * sizeof(int));
        double *val = (double *) malloc(n * sizeof(double));
        if (!ind || !val) {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }

        int cnt = 0;
        for (int i = 0; i < n; i++) {
            if (i == j) continue;
            ind[cnt] = i * n + j;
            val[cnt] = 1.0;
            cnt++;
        }

        double rhs = 1.0;
        char sense = 'E';
        int rmatbeg[1];
        rmatbeg[0] = 0;

        status = CPXaddrows(env, lp,
                            0, // nessuna nuova colonna
                            1, // una riga
                            cnt, // numero di coefficienti
                            &rhs,
                            &sense,
                            rmatbeg, // ← NON NULL
                            ind,
                            val,
                            NULL,
                            NULL);
        cpx_check(status, env, "CPXaddrows(in-degree)");

        free(ind);
        free(val);
    }

    // --- MTZ ---
    // Per i != j, i>=1, j>=1:
    // u_i - u_j + n x_{i,j} <= n-1
    //@COMPLEX@: Implementation of Miller-Tucker-Zemlin (MTZ) constraints to eliminate subtours in the TSP formulation.
    for (int i = 1; i < n; i++) {
        for (int j = 1; j < n; j++) {
            if (i == j) continue;

            int ind[3];
            double val[3];

            ind[0] = n_x + i;
            val[0] = 1.0; // u_i
            ind[1] = n_x + j;
            val[1] = -1.0; // -u_j
            ind[2] = i * n + j;
            val[2] = (double) n; // + n x_ij

            double rhs = (double) (n - 1);
            char sense = 'L';
            int rmatbeg[1];
            rmatbeg[0] = 0;

            status = CPXaddrows(env, lp,
                                0, 1, 3,
                                &rhs, &sense,
                                rmatbeg, ind, val,
                                NULL, NULL);
            cpx_check(status, env, "CPXaddrows(MTZ)");
        }
    }

    // (Opzionale) esporta modello per debug
    // CPXwriteprob(env, lp, "tsp.lp", NULL);

    // --- Ottimizzazione ---
    status = CPXmipopt(env, lp);
    cpx_check(status, env, "CPXmipopt");

    int solstat = CPXgetstat(env, lp);
    if (solstat != CPXMIP_OPTIMAL && solstat != CPXMIP_OPTIMAL_TOL) {
        fprintf(stderr, "CPLEX ended with status %d (not optimal)\n", solstat);
        // comunque proviamo a leggere quello che c'è, ma segnaliamo
    }

    double objval = 0.0;
    status = CPXgetobjval(env, lp, &objval);
    cpx_check(status, env, "CPXgetobjval");
    inst->zbest = objval;

    // --- Lettura x ---
    double *x = (double *) calloc(total_vars, sizeof(double));
    if (!x) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    // leggiamo solo le x (0 .. n_x-1)
    status = CPXgetx(env, lp, x, 0, n_x - 1);
    cpx_check(status, env, "CPXgetx");

    // --- Costruisci successori succ[i] ---
    int *succ = (int *) malloc(n * sizeof(int));
    int *pred = (int *) malloc(n * sizeof(int));
    if (!succ || !pred) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        succ[i] = -1;
        pred[i] = -1;
    }

    // per ogni i trova j tale che x_ij ~ 1
    for (int i = 0; i < n; i++) {
        int chosen = -1;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            double xv = x[i * n + j];
            if (xv > 0.5) {
                chosen = j;
                break;
            }
        }
        succ[i] = chosen;
    }

    // controlli base: ogni i deve avere un succ, e un pred unico
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (succ[i] < 0 || succ[i] >= n) {
            fprintf(stderr, "Invalid succ[%d]=%d\n", i, succ[i]);
            ok = 0;
            continue;
        }
        if (pred[succ[i]] != -1) {
            fprintf(stderr, "Node %d has multiple predecessors (%d and %d)\n",
                    succ[i], pred[succ[i]], i);
            ok = 0;
        } else {
            pred[succ[i]] = i;
        }
    }

    // --- Ricostruzione tour da 0 ---
    //@COMPLEX@: Iteratively follows the 'successor' pointers from node 0 to reconstruct the Hamiltonian cycle from the CPLEX solution.
    for (int i = 0; i < n; i++) sol[i] = (unsigned int) -1;

    int *visited = (int *) calloc(n, sizeof(int));
    if (!visited) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    int current = 0;
    for (int k = 0; k < n; k++) {
        if (current < 0 || current >= n) {
            ok = 0;
            break;
        }
        if (visited[current]) {
            fprintf(stderr, "Cycle closed early at node %d (duplicate)\n", current);
            ok = 0;
            break;
        }
        sol[k] = (unsigned int) current;
        visited[current] = 1;
        current = succ[current];
    }

    // opzionale: verifica che l'ultimo torni a 0 (ciclo)
    if (ok) {
        int last = (int) sol[n - 1];
        if (succ[last] != 0) {
            fprintf(stderr, "Tour does not return to 0: succ[last=%d]=%d\n", last, succ[last]);
            ok = 0;
        }
    }

    if (!ok) {
        fprintf(stderr, "WARNING: extracted tour seems invalid. Consider dumping tsp.lp and inspecting solution.\n");
        // puoi decidere qui cosa fare: exit(1), oppure lasciare sol parziale
    }

    // --- Cleanup ---
    free(visited);
    free(succ);
    free(pred);
    free(x);
    free(obj);
    free(lb);
    free(ub);
    free(ctype);

    CPXfreeprob(env, &lp);
    CPXcloseCPLEX(&env);
}

//TODO: We have to implement the TSP solution with the NN heuristics approach. 
//(On input you must provide the starting nodes sequence.)
//Steps:
//Init a vector with the all nodes
//Chose the starting node at random by performing the swap with (first node in the vector and actual chosen first node)
//Chose the next node based on the lowest cost from the first visited node and all the other nodes
//Iterate this procedure until you have visited the all nodes

void tspNN(instance *inst, unsigned int *sol){
    const int nodes = inst->nnodes;

    //NOTE: Init starting random node
    int node_index = rand() % nodes;

    swap(&sol[0], &sol[node_index], sizeof(int));

    double minDist = -1;
    double tmpDist = -1;
    int pointIndex = -1;

    //NOTE: The last 2 are already positioned
    for(unsigned int i = 1; i < nodes - 2; i++)
    {
        for(unsigned int j = i+1; j < nodes - 2; j++)
        {
            tmpDist = distSquared(i, j, inst);
            if(tmpDist < minDist)
            {
                minDist = tmpDist;
                pointIndex = j;
            }
        }

        swap(&sol[i], &sol[pointIndex], sizeof(int));
        minDist = -1;
        pointIndex = -1;
    }
}

