# Task 7 — Correctness of ALL implemented methods (theory + implementation)

Goal: verify that every solving method is **theoretically sound** and **correctly implemented**. This is a
method-by-method review built on top of the line-level audits (`01`–`04`), and reflects the code **after** the
`01`/`02`/`03` fixes were applied.

Verdict legend: ✅ correct · ✅* correct with a documented design simplification (not a bug) · ⚠️ caveat.

---

## 0. Methodology & empirical cross-validation

For each method I checked (a) the algorithm against its standard definition, and (b) the code line-by-line.
To back the theory, I ran an end-to-end cross-validation (random instances, n=60, seeds 1–3):

| seed | benders | callbacks | hard-fixing | local-branch | NN(0) | NN+2opt(2) | NN+VNS(3) | GRASP(5) | GRASP+2opt(7) | Extra-Mileage(10) |
|------|---------|-----------|-------------|--------------|-------|-----------|-----------|----------|---------------|-------------------|
| 1 | 62185 | 62185 | 62185 | 62185 | 66927 | 62210 | 62185 | 66803 | 62208 | 66398 |
| 2 | 64058 | 64058 | 64058 | 64058 | 70316 | 66443 | 64058 | 69417 | 65134 | 66236 |
| 3 | 60478 | 60478 | 60478 | 60478 | 66980 | 61387 | 60478 | 65495 | 62728 | 70878 |

What this proves:
- **Two independent exact methods (Benders and branch-and-cut callbacks) certify the *same* optimum** on every
  instance → the ILP model and both separation strategies are mutually corroborated.
- **Both matheuristics (hard-fixing, local-branching) reach that certified optimum** → they explore valid
  neighborhoods and accept only feasible improvements.
- **Every construction/refinement heuristic returns a cost ≥ the optimum and never below it** → tours are
  genuine Hamiltonian cycles and the cost function is consistent (an under-count would show a sub-optimal value,
  which never happens). All solutions also pass `is_solution_valid` in `main`.

---

## 1. Core data structures & helpers

### `index_upper` / `xpos` (`tsp.c:316`, `tsp.c:~1812`) — ✅
Upper-triangular edge↔column map `i*N − i(i+1)/2 + (j−i−1)` for `i<j`; `xpos` sorts the pair first. Correct
bijection onto `[0, n(n−1)/2)`, and **runtime-verified**: `build_model` asserts
`CPXgetnumcols()-1 == xpos(i,j)` for every created column. (The previously-found misuse of raw `index_upper`
on unsorted pairs was fixed in C1/HF1.)

### `build_sol` (`tsp.c:~1900`) — ✅
Extracts `succ`/`comp`/`ncomp` from a binary `xstar`. The `BUILD_SOL_CHECKS` block asserts each node has degree
2 and all vars are ~binary; the component trace follows active edges to unvisited neighbors and closes each
cycle. Correct for any integer feasible point; degree-2 guarantees the trace forms disjoint cycles.

### `succ_to_permutation` / `permutation_to_succ` (`tsp.c:44`, `tsp.c:52`) — ✅
Cycle walk from node 0. After the C3 fix `succ_to_permutation` rejects a non-Hamiltonian `succ` (subtour or
invalid link) instead of silently corrupting. `permutation_to_succ` is its exact inverse.

### `reverse_subarray_circular` (`utils.c:22`) — ✅
Reverses the half-open circular block `[l, r)` (length `r−l`, or `(N−l)+r` when wrapping). Used by 2-opt; the
combination `swap(sol[a+1],sol[b]); reverse(a+2,b)` is exactly a full reversal of `[a+1..b]` (verified by hand).

### Distance + cost + validity — ✅* (`utils.c:357`, `tsp.c:294`, `tsp.c:238`)
`calculate_solution_cost` sums consecutive edges plus the closing edge (n edges). `is_solution_valid` checks the
permutation is a Hamiltonian cycle and (after C4) compares cost with tolerance. ✅* Note (C7): distances are
nearest-integer **Euclidean**; `EDGE_WEIGHT_TYPE: ATT` files are accepted but treated as EUC_2D (an
approximation of true TSPLIB ATT) — a modeling choice, not an implementation error.

---

## 2. Construction heuristics

### Nearest Neighbor — `greedy_nn_heuristic[_iteration]` (`tsp.c:~405`, `tsp.c:571`) — ✅
Places the start at position 0, then for each position picks the nearest **unvisited** node (inner scan over
`sol[i+1..]`) and swaps it into place. Edge accounting is exact: the loop lays `n−2` edges and the two closing
edges are added explicitly → `n` edges total (verified). The outer driver shuffles start nodes, keeps the best
valid tour, and honors the time limit. ✅* minor: `for (unsigned i; i < nodes-2; …)` is only safe for `n≥2`
(degenerate `n<2` would wrap) — consistent with the solver's `n≥3` assumption everywhere.

### GRASP — same function with `GRASP_alpha>0` — ✅
Randomization multiplies the *candidate* distance by `1 + α·U(0,1)` for selection only; the accumulated
`currSol` uses the **true** distance (`realDist`). So the construction is randomized but the reported cost is
exact. `α=0` reduces to pure NN. Correct GRASP construction.

### Extra Mileage (cheapest insertion) — `extra_mileage_heuristic` (`tsp.c:491`) — ✅
Starts from the two farthest nodes, then repeatedly inserts the globally cheapest `(node, edge)` pair minimizing
`δ = d(i,k)+d(k,j)−d(i,j)` over all uninserted nodes and all tour edges — the textbook extra-mileage / cheapest
insertion criterion. Insertion position handling (including the wrap edge → append) and the array shift are
correct; the `−1` sentinel is safe because `δ ≥ 0` under the triangle inequality.

---

## 3. Refinement & metaheuristics

### 2-opt — `twoOpt_refinement[_iteration]` (`tsp.c:~628`, `tsp.c:728`) — ✅
Best-improvement: scans all non-adjacent position pairs `(i,j)`, evaluates the exact move delta
`δ = d(i,j)+d(i+1,j+1) − d(i,i+1) − d(j,j+1)`, and applies the most negative via segment reversal `[i+1..j]`.
Adjacent/circular pairs are correctly skipped. Terminates at a local optimum (or time). The incremental cost is
reconciled against `is_solution_valid` (after C4 with tolerance). Correct 2-opt.

### VNS kick (double bridge) — `vns_kick` (`tsp.c:794`) — ✅
Picks 3 distinct sorted cut points, splitting the tour into A|B|C|D, and reassembles as **A·D·C·B** keeping each
segment's internal orientation. This is a valid 4-opt double-bridge–style perturbation (always a Hamiltonian
permutation; empty trailing segments degrade gracefully to a 3-opt move). It is the standard escape move for VNS
and is not undoable by a single 2-opt step. The cost is recomputed exactly afterward. ✅* The reassembly order is
a legitimate double-bridge variant (classic texts use A·C·B·D); both are sound kicks.

### Tabu Search — integrated in 2-opt (`tsp.c:728`, tenure>0) — ✅*
When `tenure>0` the iteration accepts the best **non-tabu** move even if non-improving (correct tabu behavior),
and records the move in `tabulist`. ✅* Two deliberate simplifications, both valid but basic: (a) the tabu
attribute is the **position** index, not the edge/move identity (coarser memory); (b) there is **no aspiration
criterion** (a tabu move that would beat the incumbent is still forbidden). Neither breaks correctness — the
search stays feasible and best-so-far is tracked — they only affect search quality.

---

## 4. Exact methods (CPLEX)

### Model — `build_model` (`tsp.c:1841`) — ✅
DFJ formulation: binary `x(i,j)` for `i<j` with objective `Σ d(i,j)·x`, and `n` degree constraints
`Σ_{i≠h} x(i,h) = 2`. Column order is asserted against `xpos`. Subtour elimination is added dynamically. This is
the correct base TSP integer model.

### Benders / SEC loop — `TSPopt_benders` (`tsp.c:919`) — ✅
Solves, extracts components, and for every component `S` (with `|S|<n`) adds the SEC
`Σ_{i,j∈S} x(i,j) ≤ |S|−1`, iterating until a single tour or time-out. Correct DFJ row generation; residual time
is handled via `time_left` (T4) and bounded per solve by CPLEX's `TILIM`. Certified the same optima as the
callback method (table above).

### Branch-and-cut callbacks — `TSPopt_callbacks` + `my_callback*` — ✅
- **Candidate** (`my_callback_candidate`, `add_subtour_elimination_constraints`): on an integer candidate with
  subtours, rejects it with the violated SECs via `CPXcallbackrejectcandidate` (lazy constraints). Correct
  exactness guarantee.
- **Relaxation** (`my_callback_relaxation`): separates a fractional SEC using Concorde's global min-cut
  `CCcut_mincut`; if the min cut `< 2−ε`, the corresponding SEC is added as a user cut (`CPX_USECUT_FILTER`).
  Valid inequality, correctly only *strengthening* (never cutting off feasible tours). After the C10 fix the
  empty-edge case is guarded. ✅* It separates the single most-violated (global) cut per call rather than all
  violated SECs — a valid, common heuristic separation, not a correctness issue.
- Single-threaded during the search (C2) so the in-callback heuristic cannot race on shared state.

### Patching — `apply_patching` (`tsp.c:861`) — ✅
Merges subtours by repeatedly choosing the min-cost reconnection between two different components and splicing
their successor links (`succ[i]=next_j; succ[j]=next_i`). For two **directed** cycles this single reconnection
provably yields one consistent cycle (orientation preserved); the objective is updated by the exact delta. It is
a greedy patching heuristic (best-pair each step), so it returns *a* valid single tour, not necessarily the
cheapest patching — which is the intended role.

### Post-heuristic warm start — `post_heuristic_solution` (`tsp.c:~1010`) — ✅
From an integer candidate: deep-copies `succ`/`comp`, patches to one tour, runs time-bounded 2-opt, rebuilds the
edge vector with `xpos` (C1 fix) and posts it via `CPXcallbackpostheursoln`. The posted objective is recomputed
from distances, so it is consistent with the posted vector. Correct heuristic injection.

---

## 5. Matheuristics

### Hard fixing — `TSPopt_hard_fixing` (`tsp.c:1367`) — ✅ (after fixes)
Theory: fix a random subset of incumbent edges to 1, re-solve the restricted MIP (under SEC callbacks), accept
the (necessarily ≤) restricted optimum, unfix, repeat. The implementation now:
- seeds a real incumbent via NN+2-opt (C5/HF2);
- fixes the **correct** columns with `xpos` (C1/HF1) — previously it fixed wrong edges and aborted;
- bounds each dive's time (T2/HF3) so it performs many iterations;
- injects the incumbent as a MIP start, accepts only strict improvements, and fully restores lower bounds each
  iteration.
The restricted feasible region always contains the incumbent → monotone non-increasing and feasibility-safe.
Empirically reaches the certified optimum (table). ✅* Tunables (fix probability `p∈[0.2,0.5]` per edge, HF4) are
a quality lever, not a correctness issue.

### Local branching — `TSPopt_local_branching` (`tsp.c:~1560`) — ✅
Theory (Fischetti–Lodi soft fixing): around incumbent x̄ add `Σ_{e∈E(x̄)} x_e ≥ n−k` (≤ k edges may change),
solve, re-center on improvement, widen `k` when the neighborhood is exhausted, remove the constraint, repeat.
The implementation matches exactly: one transient row over the incumbent edge columns (built with `xpos`), sense
`'G'`, rhs `n−k`; bounded per-iteration time; MIP-start injection; **revalidates** the extracted tour
(single-component + recomputed cost) before accepting; `CPXdelrows` each iteration; adaptive `k`
(base→reset on improve, `+step` on proven-optimal-no-gain, cap at `n−1`). The incumbent always satisfies the
constraint (Σ=n ≥ n−k), so the neighborhood is non-empty and improvements are monotone. Converges to the
certified optimum (table); leak-free.

---

## 6. Residual notes (all minor, none a correctness defect)

- **Tabu**: position-based attribute, no aspiration (§3) — quality only.
- **Relaxation cut**: one global min-cut per call rather than all violated SECs (§4) — valid separation.
- **VNS reassembly**: A·D·C·B vs the classic A·C·B·D (§3) — both valid kicks.
- **ATT vs EUC_2D**: distances approximate ATT (C7) — modeling choice.
- **NN `n<2` index wrap** (§2) — outside the solver's valid input range.
- **Windows `second()`** uses CPU time (02 audit) — platform note.
- **Hard-fixing `p` schedule** (HF4) — tuning, not correctness.

---

## 7. Overall verdict

Every implemented method is **theoretically sound and correctly implemented** in the current tree. The strongest
evidence is mutual: two independent exact solvers agree on the optimum, both matheuristics reach it, and all
heuristics stay feasible at or above it across multiple instances — with all solutions passing the independent
validity check. The only open items are the explicitly-minor design simplifications listed in §6, which affect
search *quality* or *portability*, not correctness.
