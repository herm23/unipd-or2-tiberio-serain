# Task 1 — Correctness Audit

Subtle errors found by reading `main.c`, `src/tsp.c`, `src/utils.c`, headers.
Each finding lists location, what's wrong, why it matters, and a suggested fix. Ranked by severity.

Legend: 🔴 critical (wrong results / UB) · 🟠 significant · 🟡 minor / robustness.

---

## ✅ FIX STATUS — all C1–C10 applied and verified

| ID | Fix applied | Verification |
|----|-------------|--------------|
| C1 | `index_upper`→`xpos` at `tsp.c` post-heuristic + hard-fixing mipstart/fixing | hard-fixing no longer aborts; reaches optimum |
| C2 | `CPXsetintparam(env, CPX_PARAM_THREADS, 1)` in callbacks/hard-fixing/local-branching | no race; all modes still correct |
| C3 | `succ_to_permutation` now detects subtour/invalid `succ` and errors | all modes valid; no false trips |
| C4 | `is_solution_valid` uses `fabs(...) > EPSILON` | heuristic & CPLEX runs valid |
| C5 | hard-fixing now calls `matheuristic_warm_start` (warm-start filled) | warm-start 81448 → optimum 78177 |
| C6 | `CPXcloseCPLEX` on the `CPXcreateprob` error path; hardcoded mode already removed in task 5 | builds/runs clean |
| C7 | parser accepts `EUC_2D` as well as `ATT` | (EUC_2D files now load) |
| C8 | `#define DEBUG`→`#define BUILD_SOL_CHECKS` (+ matching `#ifdef`) | build is now warning-free |
| C9 | `instance inst = {0};` + default `nnodes=0` | `-f res/att48.tsp` now works (was broken) |
| C10| removed unused `prepare_temp_solution`; 2-opt invalid branch always frees+returns; `malloc(0)` guard in relaxation cut | no regressions |

**Result after fixes (random n=100 seed 3, 20 s):** callbacks/local-branching → optimum **78177**; hard-fixing
**now works** (81448 → 78177, previously crashed with `wrong degree`); benders 72657; file input fixed; build
warning-free; `leaks` reports 0 leaked bytes. Detailed before/after notes are in `06-verification.md`.

The per-finding write-ups below are retained for rationale. (Time-limit / memory fixes in `02`/`03` remain
unapplied — this round covered only the `01` correctness findings, as requested.)

---

## 🔴 C1 — `index_upper` called with i>j → out-of-bounds write (heap corruption)

`index_upper(i,j,N)` (`tsp.c:316`) computes `i*N − i(i+1)/2 + (j−i−1)` and is **only valid for i<j**.
Its parameters are `unsigned int`, so when `i>j` the sub-expression `(j-i-1)` underflows to a huge unsigned
value and the function returns a garbage index far outside `[0, ncols)`. The safe wrapper `xpos()` (`tsp.c:1583`)
exists precisely to sort the pair first — but three sites call `index_upper` **directly** on unsorted node pairs:

1. `post_heuristic_solution` — `tsp.c:1049`:
   ```c
   int j = succ_patched[i];
   int idx = index_upper(i, j, inst->nnodes);   // j may be < i
   xheu[idx] = 1.0;                              // OOB write when j<i
   ```
2. `TSPopt_hard_fixing` MIP-start build — `tsp.c:1382`:
   ```c
   int idx = index_upper(curr, next, inst->nnodes);  // curr,next from permutation, next may be < curr
   x_start_clique[idx] = 1.0;
   ```
3. `TSPopt_hard_fixing` variable fixing — `tsp.c:1403`:
   ```c
   int idx = index_upper(curr, next, inst->nnodes);  // same problem
   indices_to_change[fixed_edges_count] = idx;
   ```

Why it matters:
- A tour stored as a permutation/`succ` cycle **always** contains at least one edge whose endpoints are in
  decreasing id order (e.g. the edge closing the cycle back to node 0). So this is hit on essentially every call,
  not a rare corner case.
- Site (1) runs inside the **default** CALLBACKS mode whenever an integer candidate has subtours (the patching
  heuristic path). It is an out-of-bounds write into `xheu` (size `ncols`) → undefined behavior: heap corruption,
  intermittent crashes, or a malformed solution posted via `CPXcallbackpostheursoln(... NOCHECK)`. Note `objheu`
  itself is computed with `get_precalculated_distance` (which sorts) so the **cost** looks right while the **vector**
  is wrong — a particularly deceptive failure.
- Sites (2)/(3) make hard-fixing fix/seed the **wrong columns** (or write out of bounds), so it optimizes a
  different restricted problem than intended. This is the root reason hard-fixing is "NOT FINISHED" / broken.

Fix: replace every direct `index_upper(a,b,...)` on unsorted pairs with `xpos(a,b,inst)`. Cross-ref `04` (hard-fixing) — this is finding HF1 there too.

---

## 🟠 C2 — Multi-threaded CPLEX callbacks mutate shared `instance` (data race)

Generic callbacks run on **multiple CPLEX worker threads** by default. `my_callback_candidate` →
`post_heuristic_solution` (`tsp.c:980`) calls `twoOpt_refinement`, which writes shared instance state:
`inst->zbest`, `inst->solution` (`tsp.c:636-638`), `inst->iterations` (`tsp.c:627`), `inst->tabulist`, and even
draws to the gnuplot pipe `inst->plot`. Several threads doing this concurrently is a data race → corrupted
incumbent, garbage `solution`, or crashes.

Why it matters: nondeterministic wrong results in the default mode; hard to reproduce.

Fix options: set `CPXsetintparam(env, CPX_PARAM_THREADS, 1)` for the callback run, or make the heuristic use
only callback-local buffers and post via `CPXcallbackpostheursoln` without touching `inst`, or guard shared
writes with a mutex. (At minimum, don't run 2-opt that mutates `inst` from inside a parallel callback.)

---

## 🟠 C3 — `succ_to_permutation` produces an invalid permutation when subtours remain

`succ_to_permutation` (`tsp.c:44`) walks `succ` starting at node 0 for exactly `nnodes` steps. If `succ` still
encodes more than one subtour (`ncomp>1`), it loops inside node 0's component and **repeats nodes**, yielding a
non-permutation. In `TSPopt` the patching step runs first and reduces to a single tour before conversion
(`tsp.c:1554` → `1567`), so the normal path is safe — but:
- If CPLEX is interrupted with no usable `succ`, the `is_succ_all_zeros` guard (`tsp.c:1541`) is a fragile proxy
  (a legitimately found tour can contain `succ[k]==0`), and a near-miss can slip an invalid solution into
  `succ_to_permutation`.
- The function has no internal guard/validation.

Fix: validate single-component before converting, or have `succ_to_permutation` detect a short cycle and report.

---

## 🟠 C4 — `is_solution_valid` compares costs with exact `!=` on doubles

`tsp.c:242`: `if (expected_cost != calculate_solution_cost(inst, solution))`. Exact floating-point equality.
2-opt maintains `initial_cost` **incrementally** (`initial_cost += delta`, `tsp.c:628`) while validation
**recomputes** the sum from scratch; rounding-order differences can make these differ in the last ulp and
trip a spurious "INVALID OPTIMAL SOLUTION". Costs are integer-rounded (C7) so it usually survives, but the
comparison is still incorrect in principle and brittle if the cost model changes.

Fix: compare with tolerance, e.g. `fabs(expected - actual) > EPSILON`.

---

## 🟠 C5 — Hard-fixing has no real initial incumbent

`TSPopt(HARD_FIXING)` is entered with `inst->solution` still the **identity permutation** set in `optimize`
(`main.c:122`) — the warm-start TODO at `tsp.c:1526` is empty. Hard-fixing then fixes/seeds edges of this
arbitrary tour. Combined with C1 this is doubly broken. (Owner-approved fix: NN+2-opt warm-start before diving;
implemented as shared infra in task 5.) See `04` finding HF2.

---

## 🟡 C6 — CPLEX env/lp leaked on init-error paths; mode hardcoded

- `tsp.c:1661-1673` (post task-5 line numbers; was 1466-1478): if `CPXopenCPLEX`/`CPXcreateprob` fail,
  `print_error_and_die` exits **without** `CPXcloseCPLEX`/`CPXfreeprob`. Harmless at process exit, but the env
  handle (which may hold a license token) is not released cleanly. **Still open.** (Also in `03` M2.)
- ~~`main.c:213`: `enum CPLEX_MODE mode = CALLBACKS;` is hardcoded~~ — **RESOLVED in task 5.** `main.c:213` now
  reads `TSPopt(inst, (enum CPLEX_MODE) inst->cplex_mode)`, selectable via `-cplex_mode <benders|callbacks|
  hardfixing|localbranching>`. All four modes are now reachable from the CLI.

---

## 🟡 C7 — `EDGE_WEIGHT_TYPE` labeled `ATT` but distance is rounded Euclidean

The parser only accepts `EDGE_WEIGHT_TYPE == ATT` (`utils.c:464`) but `calculate_distance` (`utils.c:357-362`)
computes nearest-integer **Euclidean** (`int(sqrt(dx²+dy²)+0.5)`), not the TSPLIB ATT pseudo-Euclidean
(`sqrt((dx²+dy²)/10)` then a ceil/round rule). Costs won't match published ATT optima, and feeding a genuine
`EUC_2D` file is rejected by the type check even though that's what the math implements.

Fix: either implement the true ATT formula or accept/label `EUC_2D` to match the computation.

Note `calculate_squared_distance` (`utils.c:373-378`) rounds the squared value the same way — only meaningful
if a caller wants integer squared distances; currently unused on hot paths.

---

## 🟡 C8 — `build_sol` debug degree-check is always compiled in

`tsp.c:1649` does an unconditional `#define DEBUG`, so the `#ifdef DEBUG` block in `build_sol` (`tsp.c:1662-1680`)
always runs an O(n²) degree/binary validation and `print_error_and_die` on any violation, on **every**
extraction (Benders iterations, every integer callback candidate). Two issues: (a) performance overhead at scale;
(b) the macro name `DEBUG` collides conceptually with the `DEBUG` **verbosity** constant (`consts.h:17`) — confusing,
though they're different namespaces (macro vs enum value). Consider gating on an explicit build flag.

---

## 🟡 C9 — Uninitialized `instance` struct relies on scattered field-setting

`instance inst;` (`main.c:19`) is stack-allocated and never zero-initialized. `parse_command_line` sets only a
subset of fields; others (`GRASP_alpha`, `tenure`, `VNS_kick_counter`, `iterations`, `ncols`, `tbest`, `tstart`,
`plot`, `cost_plot`, …) are set later in `optimize`/readers/`TSPopt`. The current call order covers them, but any
reordering or early use reads indeterminate values. `inst.plot`/`cost_plot` in particular are only nulled inside
the readers (`utils.c:403-404, 546-547`).

**Reproduced during task 5:** `nnodes` is never defaulted in `parse_command_line`, so on the **file-input path**
(`-f res/att48.tsp` with no `-nodes_count`) the uninitialized `nnodes` holds stack garbage. When that garbage is
`> 0`, `read_input` (`utils.c:560`) sees both "has input file" and "has random params" and dies with
*"Invalid commands: please pass the input file OR the random instance"*. File input therefore works only by luck
of the garbage being ≤ 0 — a real, nondeterministic breakage, not merely theoretical. (Random-instance runs are
unaffected because `-nodes_count` sets `nnodes`.) This promotes C9 from 🟡 to effectively 🟠 for file input.

Fix: `instance inst = {0};` in `main` (and/or default `inst->nnodes = 0;` in `parse_command_line`).

---

## 🟡 C10 — Smaller items

- **`prepare_temp_solution`** (`main.c:106`) is defined but unused (dead code).
- **`twoOpt_refinement` dead else-branch** (`tsp.c:642-648`): the "invalid solution" handler only frees+returns
  when `verbosity >= SILENT`; since `SILENT==0` and verbosity is always ≥0 the guard is always true, so the
  fall-through (which would keep an invalid solution) is dead. Works today, but the intent is muddled and a future
  negative verbosity would leak `temp_solution`.
- **`twoOpt_refinement` time check skipped when `time_limit==0`** (`tsp.c:680`): with `tenure>0` (tabu) the
  local-minimum break requires `tenure==0`, so a `time_limit==0` tabu run never terminates. Not reachable in the
  current dispatch but a latent infinite loop.
- **`vns_kick` recomputes full cost** (`tsp.c:820`) via `calculate_solution_cost` (O(n)) instead of applying the
  double-bridge delta — correct but slower than necessary (author flagged with `WARNING`).
- **`my_callback_relaxation` separates cuts at every relaxation node** (`tsp.c:1184`) with no frequency/depth
  throttle, and `malloc(0)` is possible when `edge_cuts_counter==0` (`tsp.c:1204-1205`) — UB-adjacent though
  typically harmless. Consider separating only periodically and guarding the zero case.
- **`random_array_shuffle` / fixing use `rand()`** with `rand()%k` (modulo bias) and `(double)rand()/RAND_MAX`;
  fine for a prototype, noted for reproducibility/quality.

---

## Quick reference

| ID | Sev | Location | One-liner |
|----|-----|----------|-----------|
| C1 | 🔴 | tsp.c:1049,1382,1403 | `index_upper` with i>j → OOB write; use `xpos` |
| C2 | 🟠 | tsp.c:980→601 | parallel callbacks mutate shared `inst` (race) |
| C3 | 🟠 | tsp.c:44 | `succ_to_permutation` invalid when ncomp>1 |
| C4 | 🟠 | tsp.c:242 | exact `!=` double cost compare |
| C5 | 🟠 | tsp.c:1526 | hard-fixing starts from identity tour |
| C6 | 🟡 | tsp.c:1466; main.c:213 | env/lp leak on error; mode hardcoded |
| C7 | 🟡 | utils.c:357,464 | ATT label vs Euclidean math |
| C8 | 🟡 | tsp.c:1649 | debug degree-check always on |
| C9 | 🟡 | main.c:19 | struct not zero-initialized |
| C10| 🟡 | various | dead code, modulo bias, cut throttling, etc. |

**Headline:** C1 is the one to fix first — it is a real out-of-bounds write reachable in the default mode and the
root cause behind the unfinished hard-fixing. Everything else is secondary.
