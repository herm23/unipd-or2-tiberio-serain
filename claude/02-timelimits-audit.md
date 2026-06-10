# Task 2 — Time-Limit Audit (report only)

Goal: verify every timer and time-budget is correct and consistent. **No fixes applied.**

## Timing primitive

`second()` (`src/chrono.c:54`) returns **wall-clock seconds** as a `double`:
- macOS: `mach_absolute_time()` × timebase (`chrono.c:32-41`).
- Linux: `clock_gettime(CLOCK_MONOTONIC)` (`chrono.c:43-46`).
- Windows: `clock()/CLOCKS_PER_SEC` (`chrono.c:11-12`) — note this is **CPU** time on Windows, not wall-clock,
  so a multi-threaded CPLEX run would over-count elapsed time there. (🟡 platform inconsistency; non-Windows is fine.)

Units are **seconds everywhere** — no ms/s mixups found. `inst->tstart` is stamped once in `optimize`
(`main.c:128,132`) and elapsed is always `second() - inst->tstart`. Good baseline.

## Global limit

`inst->timelimit` (`utils.h:25`), default `1.0E+20` (`utils.c:251`), parsed from `-time_limit` (`utils.c:311`).
🟡 With the default (effectively infinite), any loop-until-time algorithm (VNS, tabu, hard-fixing) runs
practically forever if the user forgets `-time_limit`. Consider a sane finite default or a warning.

## Per-algorithm budgets

| Site | Elapsed / residual | Enforcement | Notes |
|------|--------------------|-------------|-------|
| `greedy_nn_heuristic` | `curr_time = second()-tstart` (tsp.c:393) | `if (curr_time > time_limit) break` (tsp.c:443) | gets `timelimit` or `timelimit*0.1` |
| `twoOpt_refinement` | `curr_time` updated per iter (tsp.c:629) | `if (time_limit!=0 && curr_time>time_limit)` (tsp.c:680) | see T1, T3 |
| `vns_kick` | `curr_time` (tsp.c:825) | `if (time_limit>0 && curr_time>time_limit)` (tsp.c:826) | **fed wrong limit — T1** |
| `TSPopt_benders` | `residual = timelimit-elapsed` (tsp.c:899-900) | `if (residual<=0) break` + `TILIM=residual` (tsp.c:902-904) + post-check (tsp.c:974) | consistent |
| `TSPopt_callbacks` | `residual` (tsp.c:1301-1302) | `if (residual<0) residual=0; TILIM=residual` (tsp.c:1303-1305) | one-shot, fine |
| `post_heuristic_solution` | `remaining = timelimit-elapsed` (tsp.c:1016-1018) | guard `<0→0`, 2-opt only if `>0` (tsp.c:1028) | fine, but see T4 |
| `TSPopt_hard_fixing` | `residual` (tsp.c:1359-1360) | `if (residual<=0.1) break` + `TILIM=residual` (tsp.c:1421) | **no per-iter cap — T2** |

---

## Findings

### 🟠 T1 — VNS kick receives the GLOBAL limit, not the residual

`tsp.c:664`:
```c
int kick_status = vns_kick(inst, inst->timelimit, temp_solution, &initial_cost);
```
Inside the refinement loop, every other check uses the local `time_limit` parameter, but the kick is handed
`inst->timelimit` (the full global budget, default 1e20). `vns_kick`'s own guard (`tsp.c:826`,
`curr_time > time_limit`) therefore almost never trips, so the kick never reports "out of time" and the loop
relies solely on the `twoOpt_refinement` post-iteration check (`tsp.c:680`). Effect: the refinement can overrun
its intended `time_limit` by up to one full (2-opt iteration + `VNS_kick_counter` kicks) batch, and with the
default infinite `timelimit` the kick's time guard is dead code.

Fix: pass the residual, e.g. `vns_kick(inst, time_limit, ...)` (or `time_limit - curr_time`), consistent with the
loop's own budget.

### 🟠 T2 — Hard-fixing sets the per-iteration CPLEX limit to the ENTIRE residual

`tsp.c:1421`: `CPXsetdblparam(env, CPX_PARAM_TILIM, residual_time)` where `residual_time` is **all** remaining
time. The first restricted MIP can therefore consume the whole budget, so the "loop" performs very few dives
(often one) — defeating the matheuristic, whose value comes from many fix/solve rounds. Standard hard-fixing
gives each subproblem a **bounded slice** (a fixed `iter_cap`, or `residual/k`, possibly with a node limit) and
loops. (Owner-approved design: capped per-iteration time — implemented for local branching in task 5; the same
cap should be applied here when C1/HF fixes land.) Cross-ref `04` HF3.

### 🟡 T3 — Time checked only after a full O(n²) 2-opt iteration (+ kicks)

`twoOpt_refinement` checks the limit at `tsp.c:680`, i.e. **after** a complete best-improvement scan and after
running up to `VNS_kick_counter` kicks. For large `n` a single iteration can overshoot the deadline noticeably.
Acceptable for a course project; note it as granularity, not a correctness break. Consider an early in-scan check
for very large instances.

### 🟡 T4 — Inconsistent residual-guard conventions across modes

Three different "out of time" tests:
- Benders: `residual_time <= 0` (`tsp.c:902`)
- Callbacks: `residual_time < 0 → clamp to 0` then proceed (`tsp.c:1303`)
- Hard-fixing: `residual_time <= 0.1` (`tsp.c:1362`)

None is wrong, but the 0.1 s magic number in hard-fixing and the differing comparison styles make behavior
near the deadline inconsistent. Recommend a single shared helper, e.g.
`double residual_time(const instance*)` returning `max(0, timelimit - (second()-tstart))`, plus one agreed
epsilon (e.g. reuse `XSMALL`/a named `TIME_EPS`).

### 🟡 T5 — `tbest` not updated on the CPLEX path

`tsp.c:1569` is a `//TODO: Update tbest`. For CPLEX modes `inst->tbest` keeps the value from `optimize`'s init
(the start time), so the reported "time of best" is meaningless for exact/matheuristic runs. Heuristics set it
correctly (`tsp.c:428,637`). Fix: stamp `inst->tbest = second()-inst->tstart` when a new CPLEX incumbent is
accepted.

### 🟡 T6 — Construction/refinement split is reasonable; nested budgets OK

`optimize` gives construction `timelimit*0.1` and refinement the **full** `timelimit` (`main.c:201-203`). Because
both use the same `tstart` baseline and absolute comparisons, the 2-opt phase correctly sees the time already
spent in construction — i.e. budgets are residual by construction, not additive. No bug here; documented to
confirm it was checked. The 10% construction share is a hardcoded heuristic (fine, just note it's not tunable).

---

## Summary

| ID | Sev | Location | Issue |
|----|-----|----------|-------|
| T1 | 🟠 | tsp.c:664 | VNS kick gets global limit, not residual |
| T2 | 🟠 | tsp.c:1421 | hard-fixing: no per-iteration cap |
| T3 | 🟡 | tsp.c:680 | time checked only after full iteration |
| T4 | 🟡 | tsp.c:902/1303/1362 | inconsistent residual guards / magic 0.1 |
| T5 | 🟡 | tsp.c:1569 | `tbest` not set for CPLEX modes |
| T6 | 🟡 | main.c:201 | construction/refinement split (OK, documented) |
| — | 🟡 | chrono.c:11 | Windows `second()` is CPU time, not wall-clock |

**Priority:** T2 (matheuristic effectiveness) and T1 (budget overrun) are the ones that change behavior; the rest
are consistency/robustness.
