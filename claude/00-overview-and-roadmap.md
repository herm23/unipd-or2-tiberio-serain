# Claude Work Folder — Overview & Roadmap

This folder holds Claude's elaboration of a 5-part job on the `unipd-or2-tiberio-serain` TSP solver.
It is documentation only; the audit findings (tasks 1–4) are **reported, not yet applied** — code changes
were deferred per the owner's decision. Task 5 (local branching) is the only implementation work.

## The job (as given)

1. Audit the source for subtle errors.
2. Audit all time limits for correctness and consistency.
3. Audit memory allocation and check for leaks.
4. Verify the hard-fixing implementation is theoretically correct.
5. Implement a version of the local-branching matheuristic.

Owner decisions that shape the work:
- **Audits: report first, fix later.** No behavioral change to audited logic yet.
- **Mode selection: add a CLI flag** (`-cplex_mode`). Today the CPLEX mode is hardcoded.
- **Matheuristics: heuristic warm-start + capped per-iteration time.**

## Deliverables in this folder

| File | Task | Contents |
|------|------|----------|
| `00-overview-and-roadmap.md` | 0 | This file: architecture map + roadmap. |
| `01-correctness-audit.md` | 1 | Subtle errors, severity-ranked. |
| `02-timelimits-audit.md` | 2 | Every timer/budget, inconsistencies. |
| `03-memory-audit.md` | 3 | alloc↔free table, leaks, lifecycle. |
| `04-hardfixing-review.md` | 4 | Theoretical correctness of hard-fixing. |
| `05-local-branching.md` | 5 | Design + implementation notes (task 5). |

## Architecture map

**Build** (`CMakeLists.txt`): C11, single executable. Sources `main.c`, `src/tsp.c`, `src/chrono.c`,
`src/utils.c`. Optionally links **CPLEX** (`IL_STD`) and **Concorde** (used for fractional-cut separation).

**Entry / control flow**
```
main()  (main.c:18)
  ├─ parse_command_line()        utils.c:244   — flags → instance
  ├─ read_input()                utils.c:555   — TSPLIB file OR random instance
  ├─ init_distances()            utils.c:72    — precompute all pairwise costs
  ├─ optimize()                  main.c:120    — set hyperparams, dispatch on model_type
  │     └─ switch(strategy):
  │         NN/GRASP ...........  greedy_nn_heuristic()        tsp.c:380
  │         *_TWO_OPT/_VNS/_TABU  greedy_nn_heuristic()+twoOpt_refinement()  tsp.c:601
  │         EXTRA_MILEAGE ......  extra_mileage_heuristic()    tsp.c:466
  │         CPLEX .............   TSPopt(inst, CALLBACKS)      tsp.c:1463   ← mode hardcoded
  ├─ is_solution_valid()         tsp.c:238
  └─ free_instance()             utils.c:151
```

**CPLEX path**
```
TSPopt()                tsp.c:1463
  ├─ build_model()      tsp.c:1589   — n(n-1)/2 binary edge vars + n degree=2 constraints
  ├─ switch(mode):
  │    BENDERS_LOOP  → TSPopt_benders()      tsp.c:892   — iterative SEC separation
  │    CALLBACKS     → TSPopt_callbacks()    tsp.c:1289  — generic callback B&C
  │    HARD_FIXING   → TSPopt_hard_fixing()  tsp.c:1331  — diving matheuristic
  ├─ (benders/callbacks) apply_patching() if subtours   tsp.c:834
  └─ build_sol()/succ_to_permutation()      tsp.c:1661 / tsp.c:44
```

**Solution representations**
- `unsigned int *solution` — tour as a permutation of node ids (canonical incumbent).
- `int *succ` — successor array (`succ[i]=j` ⇔ directed edge i→j); cycle form used by CPLEX extraction.
- `double *xstar` — edge-variable vector indexed by `xpos(i,j)`/`index_upper(i,j)`.
Converters: `succ_to_permutation` (tsp.c:44), `permutation_to_succ` (tsp.c:52), `build_sol` (tsp.c:1661).

**Edge indexing**: `xpos(i,j,inst)` (tsp.c:1583) sorts then calls `index_upper(i,j,N)` (tsp.c:316),
the upper-triangular formula `i*N − i(i+1)/2 + (j−i−1)`. **`index_upper` is only valid for i<j** — this
matters for the headline correctness bug (see `01`/`04`).

## Roadmap / status

- [x] Phase 0 — full read of `main.c`, `src/tsp.c`, `src/utils.c`, headers, `chrono.c`.
- [x] Task 1 — correctness audit → `01-correctness-audit.md` (headline: C1 OOB write via `index_upper(i>j)`).
- [x] Task 2 — time-limit audit → `02-timelimits-audit.md` (headline: T2 hard-fixing no per-iter cap, T1 VNS global limit).
- [x] Task 3 — memory audit → `03-memory-audit.md` (no steady-state leak; M1 distances over-allocated).
- [x] Task 4 — hard-fixing review → `04-hardfixing-review.md` (skeleton sound; HF1 fixes wrong columns).
- [x] Task 5 — local branching IMPLEMENTED + `-cplex_mode` flag + warm-start + per-iteration cap → `05-local-branching.md`.
      Verified: converges to the exact optimum (78177) on n=100 seed 3; 0 leaks.

**Audit fixes are still pending owner review** ("report first, fix later"). Recommended fix order once approved:
C1/HF1 (`index_upper`→`xpos`) → C9 (`= {0}`) → HF2/HF3 (reuse `matheuristic_warm_start` + add per-iter cap to
hard-fixing) → C2 (callback thread-safety) → the rest.

Verification commands live in each task file and in the approved plan
(`~/.claude/plans/your-job-on-this-replicated-manatee.md`).
