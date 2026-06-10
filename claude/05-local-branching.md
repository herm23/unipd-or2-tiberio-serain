# Task 5 — Local Branching (IMPLEMENTED)

A working Fischetti–Lodi **local-branching** (soft-fixing) matheuristic, plus the shared infrastructure it
needed: a `-cplex_mode` CLI flag and an NN+2-opt warm-start. This is the only task where code was written.

## Idea

Around the current incumbent tour x̄ (edge set E(x̄), |E(x̄)| = n), add one **soft-fixing constraint**

```
sum_{e in E(x_bar)} x_e  >=  n - k
```

i.e. at most `k` of the `n` incumbent edges may leave the tour — a bounded neighborhood of radius `k`. Solve
the resulting MIP (still under degree + subtour-elimination constraints) within a small time slice, accept any
improvement, then **remove** the constraint and repeat with an adapted `k`. Unlike hard fixing (which fixes
individual variables to 1), local branching restricts only the *number* of changes, so the model stays full and
the neighborhood is symmetric.

## What was implemented

All in `src/tsp.c`, in a `// region Local Branching` block just before `TSPopt`:

- **`inject_incumbent_mipstart(inst, env, lp)`** — warm-starts the sub-MIP with the incumbent tour via
  `CPXaddmipstarts`. Builds the column vector with **`xpos()`** (sorts the pair) so it does **not** reproduce the
  `index_upper(i>j)` bug from hard-fixing (C1/HF1).
- **`matheuristic_warm_start(inst)`** — runs `greedy_nn_heuristic` (NN) + `twoOpt_refinement` (2-opt) to seed
  `inst->solution`/`inst->zbest`. Uses 10%/20% of a finite budget, or runs to the 2-opt local optimum when the
  time limit is the default "infinite". Shared helper — hard-fixing can reuse it to fill its empty warm-start TODO.
- **`TSPopt_local_branching(inst, env, lp)`** — the main loop:
  1. register SEC callbacks (`my_callback`) for the sub-MIPs;
  2. each iteration: inject MIP start → add the LB row (`CPXaddrows`, sense `'G'`, rhs `n−k`, over the n incumbent
     columns) → set `CPX_PARAM_TILIM` to a **bounded per-iteration slice** → `CPXmipopt`;
  3. extract via `build_sol` + `succ_to_permutation`, **revalidate** the cost with `calculate_solution_cost`
     and require a single component before accepting (addresses HF5/HF7-style concerns);
  4. **delete the LB row** (`CPXdelrows`) before the next iteration (soft fixing is transient);
  5. **adapt `k`**: improved → reset to base (intensify around new incumbent); proven optimal with no gain →
     widen `k += step` (diversify); timed out → keep `k`. Stop on time limit or when `k` exceeds its cap.

Tunables (`#define` at the top of the region): `LB_K_BASE=10`, `LB_K_STEP=10`, `LB_K_MAX=50`,
`LB_ITERS_TARGET=10`, `LB_MIN_ITER_TIME=1.0s`, `LB_TIME_EPS=0.1s`. `k` is also capped at `n−1` so `rhs = n−k ≥ 1`.

### Wiring (shared infra)

- `headers/tsp.h` — added `LOCAL_BRANCHING` to `enum CPLEX_MODE`.
- `headers/utils.h` — added `int cplex_mode;` to `instance`.
- `src/utils.c` — `parse_command_line` defaults `cplex_mode = CALLBACKS` and parses
  `-cplex_mode <benders|callbacks|hardfixing|localbranching>`; echoes it in the parameter dump.
- `main.c` — replaced the hardcoded `mode = CALLBACKS` with `TSPopt(inst, inst->cplex_mode)`.
- `src/tsp.c` — added the `LOCAL_BRANCHING` case (warm-start then local branching). The post-switch
  patching/conversion block runs only for BENDERS/CALLBACKS, so LB (which already holds a finished incumbent)
  correctly skips it.

**Scope note:** per the owner's "report first, fix later" decision, `TSPopt_hard_fixing`'s body was **not**
modified — its HF1/C1 index bug and missing warm-start remain documented, not patched. The reusable warm-start
helper is ready for it once those fixes are approved.

## Reused (no new primitives)

`xpos`/`index_upper`, `build_sol`, `succ_to_permutation`, `calculate_solution_cost`, `greedy_nn_heuristic`,
`twoOpt_refinement`, `my_callback`, and the `CPXaddmipstarts` pattern.

## Verification (run and observed)

Build: CLion's bundled cmake/ninja against the configured CPLEX 22.12 + Concorde (`cmake-build-debug/`).
Compiles cleanly (only the pre-existing `DEBUG`-macro-redefined warning, finding C8).

**Convergence to the exact optimum** — random n=100, seed 3, 20 s:
- Exact reference (`-cplex_mode callbacks`): optimum **78177** (0.31 s).
- Local branching: warm-start (NN+2opt) **81448** → **79546** → **78654** → **78177** (k=10, 0.52 s),
  "Finished after 8 iterations. Best cost: 78177." Monotone, ends ≤ warm-start and = optimum; tour validated by
  `is_solution_valid` in `main`. `tbest` is set correctly (0.517 s) on this path — incidentally avoiding T5.

**Flag routing** — same instance family:
- `benders` → 72657, valid; `callbacks` → optimum, valid; `hardfixing` → dispatches but dies with
  `wrong degree in build_sol()`, **empirically confirming HF1/C1** (it fixes the wrong columns). Untouched by design.

**Memory** — `leaks --atExit` on the local-branching path (n=60, seed 5, 6 s): **0 leaks for 0 total leaked bytes.**

### Reproduce

```sh
CMAKE=$(ls ~/Applications/CLion*.app/Contents/bin/cmake/mac/*/bin/cmake | head -1)
"$CMAKE" --build cmake-build-debug

# Local branching vs exact reference (random instances avoid the C9 uninitialized-nnodes issue with -f):
./cmake-build-debug/unipd_or2_tiberio_serain -nodes_count 100 -seed 3 -model 11 -cplex_mode callbacks     -time_limit 30 -v 1 --no-plot
./cmake-build-debug/unipd_or2_tiberio_serain -nodes_count 100 -seed 3 -model 11 -cplex_mode localbranching -time_limit 20 -v 2 --no-plot
```

## Possible extensions

- Couple `k` adaptation with the per-iteration node limit (Fischetti–Lodi use a node cap + diversification
  branch `sum x_e <= n - k - 1` on the *complement* to force a different region).
- Apply `apply_patching` if a timed-out sub-MIP ever returns subtours (currently such a result is skipped).
- Feed the final incumbent back as a MIP start to a final exact `callbacks` solve to certify optimality.
- Note (T1/T2 style): with the default infinite `-time_limit`, the per-iteration cap collapses to the whole
  residual — always pass a finite `-time_limit` for the intended diving behavior.
