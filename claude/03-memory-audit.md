# Task 3 — Memory Audit (report only)

Goal: enumerate allocations, pair them with frees, identify leaks and lifecycle issues. **No fixes applied.**

Overall the code is **disciplined about freeing** — the great majority of allocations have a matching `free` on
all normal paths. The issues below are (a) one genuine over-allocation, (b) leaks only on `print_error_and_die`
exit paths (harmless because the process dies), and (c) hygiene items. No unbounded/growing leak was found on
the steady-state heuristic paths.

## alloc ↔ free pairing (steady state)

| Alloc | Site | Freed |
|-------|------|-------|
| `distances` | utils.c:73 | free_instance utils.c:154 |
| `xcoord`/`ycoord` | utils.c:455-456 / 521-522 | free_instance utils.c:152-153 |
| `solution` | main.c:122 | free_instance utils.c:155 |
| `tabulist` | main.c:145 | free_instance utils.c:156 |
| `nodes_order`,`temp_solution` (NN) | tsp.c:385,389 | tsp.c:455,441 |
| `sol`,`visited` (NN iter / extra-mileage) | tsp.c:470-471 | tsp.c:532-533 |
| `temp_solution` (2-opt) | tsp.c:612 | tsp.c:645/658/682 (all exits) |
| `temp_solution` (vns_kick) | tsp.c:807 | tsp.c:823 |
| callback `xstar/succ/comp` | tsp.c:1153,1162-63 | tsp.c:1176-78 |
| relaxation buffers | tsp.c:1188,1204-05,1231-32,1239 | tsp.c:1261-70 + `CC_IFFREE(cut)` 1267 |
| SEC buffers | tsp.c:1090,1096-97 | tsp.c:1143-45 |
| post-heuristic buffers | tsp.c:986-87,1008,1044,1059 | tsp.c:1074,1081-84 |
| Benders buffers | tsp.c:1491-1500 | tsp.c:1505-09 + 1572-73 |
| callbacks buffers | tsp.c:1515-17 | tsp.c:1521,1572-73 |
| hard-fixing buffers | tsp.c:1343-51 | tsp.c:1455-60 |
| hard-fixing per-iter `x_start_clique`,`mipstart_ind` | tsp.c:1378,1389 | tsp.c:1393-94 (same iter) |
| build_model `cname`,`index`,`value` | tsp.c:1593-94,1614-15 | tsp.c:1637-43 |
| CPLEX env/lp | tsp.c:1466,1473 | tsp.c:1576-77 |
| `swap` scratch | utils.c:125 | utils.c:131 |

Verdict: **paired on all normal exits.**

---

## Findings

### 🟠 M1 — Distance matrix over-allocated 2× (waste, not a leak)

`init_distances` (`utils.c:73`):
```c
inst->distances = malloc(inst->nnodes * inst->nnodes * sizeof(double)); //TODO fix this
```
Only the upper triangle is ever used (indexing via `index_upper`, max index `n(n-1)/2 − 1`). Allocating `n²`
doubles wastes ~50% (for n=5000 that's ~200 MB vs ~100 MB). The author flagged it (`//TODO fix this`). Also:
- **No `i*n` overflow guard**: `nnodes*nnodes` is `int*int`; for n ≳ 46341 this overflows `int` before promotion
  to `size_t`, giving a too-small allocation → later OOB. (Unlikely at course sizes, but real.)
- Correct size is `(size_t)n*(n-1)/2 * sizeof(double)`.

### 🟡 M2 — CPLEX env/lp not released on init-error paths

`tsp.c:1466-1478`: on `CPXopenCPLEX`/`CPXcreateprob` failure the code calls `print_error_and_die` (→ `exit(1)`)
without `CPXcloseCPLEX`/`CPXfreeprob`. The OS reclaims memory, but a CPLEX **license token** may not be released
cleanly. Same pattern for the many `print_error_and_die` calls inside `build_model`, Benders, and callbacks: they
abandon in-scope buffers. All are fatal-exit paths, so these are "leaks on the way out the door," not steady-state
leaks. Listed for completeness; only worth fixing if you want clean teardown / are running under leak-detectors.

### 🟡 M3 — Per-iteration alloc churn in hard-fixing

`x_start_clique` and `mipstart_ind` are malloc'd and freed **inside** the hard-fixing loop (`tsp.c:1378-1394`).
Correct (no leak), but they're fixed-size (`ncols`) and could be hoisted above the loop and reused, like the
other hard-fixing buffers already are (`tsp.c:1343-1351`). Pure efficiency/style.

### 🟡 M4 — No `malloc` NULL-checks on hot allocations

`init_distances` (utils.c:73), `initialize_identity_solution` returns NULL but callers don't all check, NN/2-opt
buffers, etc. don't verify the return. On allocation failure these dereference NULL → crash rather than a clean
error. `generate_random_instance` does check (utils.c:524). Low priority at course sizes; note for robustness.

### 🟡 M5 — `free_instance` doesn't null freed pointers; double-free window

`free_instance` (utils.c:151) frees but doesn't set pointers to NULL. `main.c` calls it on both the invalid-solution
error path (main.c:40) and normal exit (main.c:86) — but the error path `print_error_and_die`s immediately after,
so no double-free occurs today. Still, nulling after free (and guarding) is cheap insurance. Also note `main.c:81`
`pclose(inst.plot)` then sets `inst.plot=NULL`, so `free_instance`'s `pclose` (utils.c:157) is correctly skipped —
good, no double-close.

### 🟡 M6 — `malloc(0)` possibility in relaxation callback

`tsp.c:1204-1205`: `malloc(2*edge_cuts_counter*…)` / `malloc(edge_cuts_counter*…)` when `edge_cuts_counter==0`.
`malloc(0)` is implementation-defined (may return NULL or a unique pointer); subsequent loops don't execute, and
the frees handle NULL, so it's safe in practice but worth a guard. (Also noted as C10.)

---

## Recommended empirical check

The static review finds no steady-state leak, but confirm with a tool. On this macOS box:

```sh
# Build first (needs CPLEX). Then, e.g. for the heuristic path (no CPLEX needed at runtime):
leaks --atExit -- ./build/unipd_or2_tiberio_serain -nodes_count 200 -seed 1 -time_limit 5 -model 3 --no-plot
# Or Valgrind on Linux:
valgrind --leak-check=full --show-leak-kinds=all \
  ./build/unipd_or2_tiberio_serain -nodes_count 200 -seed 1 -time_limit 5 -model 3 --no-plot
```
Expect: clean for heuristic modes; for CPLEX modes, any "still reachable" under CPLEX/Concorde internals is the
library's, not ours — focus on our buffers. Re-run after the C1 (`index_upper`) fix, since the OOB write there can
also corrupt the heap and surface as spurious allocator errors.

---

## Summary

| ID | Sev | Location | Issue |
|----|-----|----------|-------|
| M1 | 🟠 | utils.c:73 | distances over-allocated `n²`; int-overflow at large n |
| M2 | 🟡 | tsp.c:1466+ | env/lp + buffers leaked on fatal-exit paths |
| M3 | 🟡 | tsp.c:1378 | hard-fixing per-iter alloc churn (hoistable) |
| M4 | 🟡 | utils.c:73 etc. | missing malloc NULL-checks |
| M5 | 🟡 | utils.c:151 | freed pointers not nulled |
| M6 | 🟡 | tsp.c:1204 | possible `malloc(0)` |

**No growing memory leak detected on normal runs.** M1 is the only substantive item (memory waste + a real
large-n overflow); the rest are hygiene.
