# Task 4 — Hard-Fixing Theoretical Review (report only)

Subject: `TSPopt_hard_fixing` (`tsp.c:1331-1461`) + helper `update_fixing_probability` (`tsp.c:27`).
Goal: assess whether the implementation is a theoretically correct hard-fixing (diving) matheuristic.
**No fixes applied.**

## What hard-fixing should be

Given an incumbent tour x̄ (a feasible TSP solution), repeatedly:
1. **Fix** a subset of incumbent edges to 1 (`x_e = 1`), leaving the rest free.
2. **Solve** the restricted MIP (still under degree + SEC constraints) to optimality or a time/node cap.
3. The restricted optimum is ≤ the incumbent (the incumbent stays feasible), so accept it as the new incumbent.
4. **Unfix**, re-randomize the fixed set, and repeat until the global time limit.

Because each restricted problem contains the incumbent in its feasible region, the method is **monotone
non-increasing** in cost and never loses feasibility. Fixing more edges → smaller/easier subproblem but smaller
neighborhood; fixing fewer → larger neighborhood, harder subproblem.

## What the code actually does — structural walk-through

- Registers SEC callbacks on the sub-problem (`tsp.c:1338`) so each restricted solve enforces subtour
  elimination. ✅ Correct and necessary (the base model has only degree constraints).
- Loop guard `residual_time <= 0.1` (`tsp.c:1362`). ✅ Terminates on the global budget.
- `update_fixing_probability` every 10 iters → `p ~ U[0.2,0.5]` (`tsp.c:27-38`). ⚠️ see HF4.
- Deletes prior MIP starts, injects the incumbent as a MIP start (`tsp.c:1373-1392`). ✅ Conceptually right
  (warm start), ❌ but built with the buggy index (HF1).
- Fixes each incumbent edge independently with probability `p` via `CPXchgbds(... 'L' ...=1.0)` (`tsp.c:1396-1413`).
  ✅ Mechanism (raise lower bound to 1) is the correct way to fix a binary to 1, ❌ but indices are wrong (HF1).
- Solves with `TILIM = residual_time` (`tsp.c:1421`). ❌ no per-iteration cap (HF3).
- On success extracts via `build_sol` + `succ_to_permutation`, accepts if `sub_objval < zbest`
  (`tsp.c:1426-1439`). ✅ monotone acceptance, ⚠️ trusts `CPXgetobjval` (HF5).
- Restores lower bounds to 0 (`tsp.c:1443-1449`). ✅ Correct unfix; bounds were only ever changed for the fixed
  set, so resetting those same `indices_to_change` to LB=0 fully relaxes. (Upper bounds untouched = 1. ✅)

So the **skeleton is theoretically sound** — fix-via-LB, SEC callbacks, monotone accept, proper unfix. The
problems are in the details that make it actually correct/effective.

---

## Findings

### 🔴 HF1 — Fixed/seeded columns are computed with the wrong index (same as C1)

`tsp.c:1382` (MIP start) and `tsp.c:1403` (fixing) use
```c
int idx = index_upper(curr, next, inst->nnodes);   // curr,next adjacent nodes in the tour
```
`index_upper` is valid only for `curr < next`; for `curr > next` the unsigned subtraction underflows and the
index is garbage / out of range. Consequences:
- `x_start_clique[idx] = 1.0` (`tsp.c:1383`) → out-of-bounds write building the MIP start.
- `indices_to_change[...] = idx` → `CPXchgbds` fixes the **wrong column** (or an invalid one), i.e. it fixes
  edges that are not in the incumbent. The restricted problem is then **not** a neighborhood of the incumbent,
  breaking the monotonicity guarantee entirely — the method is theoretically invalid as written.

Fix: use `xpos(curr, next, inst)` (which sorts) at both sites. This is the single most important correctness fix
for hard-fixing.

### 🟠 HF2 — No initial incumbent (starts from identity tour) (same as C5)

The warm-start TODO at `tsp.c:1526` is empty, so `inst->solution` is still the identity permutation from
`optimize` (`main.c:122`). Hard-fixing therefore "dives" around an arbitrary, very poor tour, and `inst->zbest`
is the identity cost. Theory assumes x̄ is a *good* feasible incumbent; a poor seed both slows convergence and,
with HF1, fixes meaningless edges. Owner-approved fix: run NN + 2-opt first (shared warm-start helper, task 5).

### 🟠 HF3 — No per-iteration time cap (same as T2)

`TILIM = residual_time` (`tsp.c:1421`) gives the **whole** remaining budget to a single restricted MIP, so the
loop typically performs one or very few dives. Hard-fixing's benefit is *many* fast restricted solves; without a
cap it degenerates into "solve once with some edges fixed." Use a bounded slice per iteration (fixed `iter_cap`,
or a fraction of residual, optionally a CPLEX node limit), capped at the residual. Owner-approved design.

### 🟠 HF4 — Fixing probability is low and its semantics are worth confirming

`p ~ U[0.2,0.5]` means each incumbent edge is fixed with probability 20–50%, so on average **50–80% of edges
remain free**. That is a *large* free neighborhood → restricted MIPs nearly as hard as the full problem, which
compounds HF3 (one slow solve). Classic hard-fixing fixes a *high* fraction (often ~70–90%) so each subproblem
is small and fast. Recommend raising the fix fraction (e.g. `p ∈ [0.5, 0.8]`) and tuning against instance size.
Also `update_fixing_probability` only changes `p` every 10 iterations (`tsp.c:29`) — combined with HF3 (few
iterations) `p` may **never** update in a typical run, so the schedule is effectively dead. Re-evaluate the
adaptation cadence once per-iteration capping (HF3) yields many iterations.

### 🟡 HF5 — Accepts `CPXgetobjval` without revalidating the tour

`tsp.c:1430-1434` reads `sub_objval` from CPLEX and, if it beats `zbest`, stores the permutation. It never
recomputes the cost from the extracted tour (`calculate_solution_cost`) nor validates a single component. With
SEC callbacks the returned candidate should be a valid tour, but on a timed-out solve CPLEX may return its best
integer incumbent — defensively, recompute and validate before accepting (the exact-method post-processing does
exactly this at `tsp.c:1568`). Cheap insurance against silently accepting a bad cost.

### 🟡 HF6 — No diversification / acceptance of sidesteps

When an iteration yields no improvement, the only "diversification" is the random re-draw of the fixed set
(and the occasional `p` change). There's no escalation (e.g. reduce fix fraction, perturb the incumbent) and no
acceptance of equal-cost moves. Acceptable for a basic version; note as a quality lever. The loop also performs
no logging of "no-improvement streaks," which would help tuning.

### 🟡 HF7 — Post-timeout extraction can hit C3

If a restricted solve times out with subtours in `xstar` (shouldn't happen under working SEC callbacks, but
possible if callbacks are bypassed), `build_sol` would yield `ncomp>1` and `succ_to_permutation` (`tsp.c:1435`)
would produce an invalid permutation (see C3). Guard with a single-component check before accepting.

---

## Verdict

The **algorithmic skeleton is theoretically correct** (fix-to-1 via lower bounds, SEC-enforced restricted solves,
monotone acceptance, correct unfix). It is **not correct as implemented** because:
- HF1 fixes the **wrong columns** (unsigned index bug) → the restricted problem is not a neighborhood of the
  incumbent, voiding the monotonicity/feasibility guarantee. **Must fix first.**
- HF2 (no warm start) and HF3 (no per-iteration cap) make it ineffective even once HF1 is fixed.
- HF4 (low fix fraction + dead `p` schedule) and HF5–HF7 are correctness/effectiveness refinements.

Recommended order: **HF1 → HF2 → HF3 → HF4 → HF5**. After HF1–HF3 the method should produce monotone improving
incumbents within the time budget. These map directly onto the shared infrastructure built for local branching
(task 5): the `xpos` fix, the NN+2-opt warm-start helper, and the per-iteration time cap are reused by both.
