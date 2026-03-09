# GNUPLOT
## Phase 1 - Prompt optimization (with PromptEngineering GPT)

```md
Improve my prompt:  
I'm working with C (plain) (standard C11). 
I have a datastructure like 

typedef struct {
int nnodes; double *xcoord; double *ycoord; ... 
} instance;

and I have to use GNUplot to plot the solution for an instance.

Solution is a vector of size nnodes and is simply a permutation of the indexes of the nodes.

This function must plot the nodes and the segments of the tour (TSP problem) given the solution.

This funcion called plot_tour must also have a flag only_instance to disable the plot of the edges in order to visualize only the points.
```

We obtained:

```
You are an expert C (C11 standard) developer.

I have the following data structure:

typedef struct {
int nnodes;
double *xcoord;
double *ycoord;
} instance;

I am solving a TSP problem, and I have a solution array of size nnodes that contains a permutation of node indices representing the tour order.

Task

Write a C11-compatible function:

void plot_tour(const instance *inst, const int *solution, int only_instance);
Requirements

Use GNUplot to generate the visualization (via pipe using popen()).

If only_instance == 1:

Plot only the nodes as points.

If only_instance == 0:

Plot nodes as points

Plot the tour edges in the order given by solution

The tour must be closed (last node connects to first)

The function must:

Be fully C11 compliant

Handle basic error checking (null pointers, popen failure)

Not leak memory

Use clear plotting style (e.g., points and lines with reasonable formatting).

Output Format

Provide:

The full function implementation.

A short explanation of how the GNUplot pipe works.

Any assumptions made.

Keep the solution clean, robust, and production-ready.
```


## Phase 2 - Prompt calling on GPT 5.2 Pro

```c
void plot_tour(const instance *inst, const int *solution, int only_instance)
{
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
    fprintf(gp, "set size ratio -1\n");  /* keep aspect ratio */

    if (only_instance) {

        /* ---- Plot only nodes ---- */
        fprintf(gp, "plot '-' with points pt 7 ps 1.5 lc rgb 'blue'\n");

        for (int i = 0; i < inst->nnodes; ++i) {
            fprintf(gp, "%f %f\n", inst->xcoord[i], inst->ycoord[i]);
        }

        fprintf(gp, "e\n");
    }
    else {

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
```

