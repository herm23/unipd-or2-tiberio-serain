# Task — Verification of points 00–05 against the CURRENT code

Re-checked every point after the task-5 edits. Method: grep/read each anchor in the working tree and confirm the
described behavior still holds. **Note on line numbers:** task 5 inserted the local-branching block (~195 lines)
into `src/tsp.c` immediately before `TSPopt`, and ~16 lines into `src/utils.c`'s `parse_command_line`. So audit
references *after* those points drifted. The tables below give the **current** line numbers. Findings themselves
are unchanged unless marked.

Status: ✅ confirmed (real, accurately described) · ✏️ confirmed, line number corrected · ⚠️ status changed.

## 00 — Overview/roadmap
- ✅ Architecture map + build path accurate. Roadmap checkboxes updated (all tasks done). Recommended fix order present.

## 01 — Correctness audit

| ID | Status | Current location | Re-check result |
|----|--------|------------------|-----------------|
| C1 | ✅ | `tsp.c:1049` (post_heuristic), `1382`,`1403` (hard-fixing); `xpos` now at **`tsp.c:1785`** | Real. **Empirically proven**: `-cplex_mode hardfixing` aborts with `wrong degree in build_sol()` — the wrong columns get fixed. Local branching (new code) uses `xpos` at `1488`/`1587` and does NOT exhibit it. |
| C2 | ✅ | `post_heuristic_solution` `tsp.c:980` → `twoOpt_refinement` `tsp.c:601`; shared writes `tsp.c:627,636-638` | Real. Callback runs 2-opt that mutates `inst->{zbest,solution,iterations,tabulist}`; race under multi-thread B&C. |
| C3 | ✏️ | `succ_to_permutation` `tsp.c:44`; guard `is_succ_all_zeros` now **`tsp.c:1743`**; patching call **`1756`**; post-conv **`1769`** | Real. Single-tour assumption holds; guard is still a fragile proxy. |
| C4 | ✅ | `tsp.c:242` (exact `!=`); incremental cost `tsp.c:628` | Real. Exact double compare vs incrementally-maintained cost. |
| C5 | ✏️ | identity init `main.c:122`; empty warm-start TODO now **`tsp.c:1721`** | Real. Hard-fixing still starts from identity (its body untouched per report-first). |
| C6 | ⚠️ | env `tsp.c:1661`, lp `tsp.c:1668` (no close on error) | **Split outcome:** (a) env/lp-leak-on-error **still real** (now `tsp.c:1661-1673`). (b) "mode hardcoded at main.c:213" is **RESOLVED** by task 5 — `main.c:213` now reads `TSPopt(inst, inst->cplex_mode)`. See inline note added to `01`. |
| C7 | ✏️ | `calculate_distance` now **`utils.c:368-375`** (round at `374`); ATT check **`utils.c:479`**; `calculate_squared` **`utils.c:386-391`** | Real. Labeled ATT, computes rounded Euclidean. |
| C8 | ✏️ | `#define DEBUG` now **`tsp.c:1851`**; `build_sol` `#ifdef DEBUG` block **`tsp.c:1864-1882`** | Real. Degree-check always compiled in; warning still emitted by the build (seen in `cmake --build`). |
| C9 | ✅ | `instance inst;` `main.c:19`; read_input conflict now **`utils.c:574`** | Real and **reproduced**: `-f res/att48.tsp` dies with "Invalid commands…" because uninitialized `nnodes` is garbage. |
| C10| ✅ | `prepare_temp_solution` `main.c:106`; 2-opt dead branch `tsp.c:642-648`; time guard `tsp.c:680`; vns full-cost `tsp.c:820`; relaxation `tsp.c:1184`, malloc(0) `tsp.c:1204-1205` | All real, all unchanged (before insertion point). |

## 02 — Time-limit audit

| ID | Status | Current location | Re-check result |
|----|--------|------------------|-----------------|
| T1 | ✅ | `tsp.c:664` | Real. `vns_kick(inst, inst->timelimit, …)` passes the global limit. |
| T2 | ✅ | `tsp.c:1421` | Real. Hard-fixing sets `TILIM = residual_time` (whole residual), no per-iter cap. |
| T3 | ✅ | `tsp.c:680` | Real. Time checked only after a full O(n²) iteration + kicks. |
| T4 | ✅ | benders `tsp.c:902`, callbacks `tsp.c:1303`, hard-fix `tsp.c:1362` | Real. Three different residual conventions / magic 0.1. |
| T5 | ✏️ | `//TODO: Update tbest` now **`tsp.c:1771`** | Real for benders/callbacks/hard-fixing. **Note:** the new local-branching path *does* set `tbest` correctly (`tsp.c:1611`), so LB is exempt. |
| T6 | ✅ | `main.c:200-203` | Real (documented as OK). Construction gets `timelimit*0.1`, refinement full; budgets are residual by shared `tstart`. |
| — | ✅ | `chrono.c:54` (and `11` Windows) | `second()` wall-clock on mac/linux; Windows uses CPU time. |

## 03 — Memory audit

- ✅ **No steady-state leak** — re-confirmed: `leaks --atExit` on the local-branching path reports `0 leaks for 0 total leaked bytes`.
- ✏️ **M1** `utils.c:73` (unchanged) — `n²` over-allocation + `int` overflow at large n. Real.
- ✏️ **M2** env/lp + buffers leaked on fatal-exit — now `tsp.c:1661,1668`; `CPXfreeprob`/`CPXcloseCPLEX` at `tsp.c:1778-1779`. Real.
- ✅ **M3** hard-fixing per-iter alloc churn `tsp.c:1378-1394` (unchanged). Real.
- ✅ **M4** missing malloc NULL-checks. Real.
- ✅ **M5** `free_instance` `utils.c:151` doesn't null pointers. Real.
- ✅ **M6** possible `malloc(0)` `tsp.c:1204-1205`. Real.
- ✏️ alloc↔free **table**: entries inside `TSPopt`/`build_model` shift by ~+195 (e.g. build_model now `tsp.c:1791`, Benders buffers `~1686-1709`, callbacks buffers `~1710-1712`, env/lp `1661/1668/1778/1779`). Hard-fixing buffers `tsp.c:1343-1351` unchanged. Pairing conclusions unchanged.

## 04 — Hard-fixing review

| ID | Status | Current location | Re-check result |
|----|--------|------------------|-----------------|
| HF1 | ✅ | `tsp.c:1382,1403` | Real, **proven** (degree-error abort). Root cause. |
| HF2 | ✏️ | empty warm-start TODO **`tsp.c:1721`**; identity `main.c:122` | Real. (Shared `matheuristic_warm_start` now exists at `tsp.c:1503`, ready to reuse.) |
| HF3 | ✅ | `tsp.c:1421` | Real. No per-iteration cap (same as T2). |
| HF4 | ✅ | `update_fixing_probability` `tsp.c:27-38` | Real. `p∈[0.2,0.5]` (low fix fraction); 10-iter cadence likely never fires given HF3. |
| HF5 | ✏️ | accept `tsp.c:1431-1435`; exact-path recompute **`tsp.c:1770`** | Real. Trusts `CPXgetobjval` without revalidating the tour. |
| HF6 | ✅ | (whole function) | Real. No diversification beyond random re-draw. |
| HF7 | ✏️ | extraction `tsp.c:1435` | Real. Post-timeout subtour extraction could hit C3. |
- Verdict (“skeleton sound, broken by HF1/HF2/HF3”) re-confirmed.

## 05 — Local branching (implementation)

All structural claims verified present + behavior tested:

| Claim | Status | Evidence |
|-------|--------|----------|
| `-cplex_mode` flag (enum + struct + parse + default + main wiring) | ✅ | `tsp.h:52`, `utils.h:54`, `utils.c:254,322-330,348`, `main.c:213` |
| `inject_incumbent_mipstart` uses `xpos` (avoids C1) | ✅ | `tsp.c:1477`, `xpos` at `1488` |
| `matheuristic_warm_start` (NN+2opt) | ✅ | `tsp.c:1503`; observed warm-start cost 81448 |
| `TSPopt_local_branching` (Σx_e≥n−k, sense `'G'`, transient row, capped per-iter time, adaptive k) | ✅ | `tsp.c:1532`; `CPXaddrows`…`'G'` `1593`, `CPXsetdblparam(…per_iter)` `1597`, `k+=LB_K_STEP` `1635`, `CPXdelrows` `1641` |
| `LOCAL_BRANCHING` case = warm-start + LB | ✅ | `tsp.c:1728-1731` |
| Post-switch patching/conv skipped for LB | ✅ | guard `tsp.c:1740` (`BENDERS_LOOP||CALLBACKS` only) |
| Converges to exact optimum | ✅ | n=100 seed 3: 81448→79546→78654→**78177** = callbacks optimum; tour valid |
| No leaks | ✅ | `0 leaks for 0 total leaked bytes` |
| Build clean | ✅ | only pre-existing `DEBUG`-redefine warning (C8) |

## Overall

- **Tasks 1–4 (audits):** every finding re-verified as real against the current tree. Corrections are limited to
  drifted line numbers (tabulated above) and one status change: **C6's "mode hardcoded" is now resolved** by task 5.
- **Task 5 (local branching):** fully implemented and behaving correctly (converges to optimum, leak-free, flag-routed).
- Audit code fixes remain intentionally **unapplied** (report-first), except where task-5 infrastructure
  incidentally resolved C6's hardcoding and exempted the LB path from T5.
