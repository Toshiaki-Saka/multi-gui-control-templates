# Algorithms — overview

[Docs index](README.md) · [日本語](../ja/algorithms.md)

Conventions shared by all four cores, and a side-by-side comparison of the
numerical methods they use. **The full derivation for each module lives in its
own folder:**

| Module | Subject | Detail |
|--------|---------|--------|
| [`pid`](pid/theory.md) | PID control of a 1-DOF attitude loop | [pid/theory.md](pid/theory.md) |
| [`mass_spring_damper`](mass_spring_damper/theory.md) | Mass-spring-damper forced response | [mass_spring_damper/theory.md](mass_spring_damper/theory.md) |
| [`pi_path_tracking`](pi_path_tracking/theory.md) | PI path following of a 3-DOF planar vehicle | [pi_path_tracking/theory.md](pi_path_tracking/theory.md) |
| [`two_dof`](two_dof/theory.md) | 2-DOF control vs plain PID | [two_dof/theory.md](two_dof/theory.md) |

---

## 0. Common conventions

**Saturation.** All cores use the same clamp,

$$\mathrm{sat}(v, a, b) = \max\bigl(a,\ \min(b,\ v)\bigr)$$

written symmetrically as $`\mathrm{sat}(v, \pm L)`$ when the bound is $[-L, L]$.
A limit of $0$ means *disabled* in `pid`; in `track` the limits are required to
be strictly positive.

**Time grids.** Three of the four cores emulate a NumPy `arange` from the
reference script, because the sample count is part of the numerical parity
contract:

| Core | Python expression | C++ count |
|------|-------------------|-----------|
| `pid` | `range(1, time_length)` | $N = \texttt{time\_length}$ (index 0 holds the initial state) |
| `msd` | `np.arange(0.0, stop + dt, dt)` | $`N = \lfloor (t_{\rm stop}+\Delta t)/\Delta t - 10^{-12}\rfloor + 1`$ |
| `tdof` | `np.arange(0, t_end, dt)` | $`N = \lceil t_{\rm end}/\Delta t - 10^{-12}\rceil`$ |
| `track` | fixed step count | $N = \lfloor T/h \rfloor$ |

The $10^{-12}$ guard absorbs floating-point noise so that an exactly divisible
horizon does not gain or lose a spurious final sample.

**Input validation.** Every `*_simulate()` returns `NULL` instead of throwing
when the configuration is not finite or violates a domain constraint
(`m > 0`, `dt > 0`, `time_length ≥ 2`, …). Frontends surface that as an error
message; no partial results are produced.

**Determinism.** No random numbers, no adaptive stepping, no threading inside
the cores. The same config always yields bit-identical output, which is what
lets the smoke tests pin exact decimal values.

---

## 5. Method comparison

| | `pid` | `msd` | `track` | `tdof` |
|---|---|---|---|---|
| Model | discrete accumulator | 2nd-order ODE | 6-state nonlinear ODE | LTI transfer functions |
| Controller | PID, clamps | none (open loop) | PI speed + PI lateral/yaw + rate damping | PID, optional pre-filter |
| Method | explicit recurrence | fixed-step vector RK4 | per-state RK4 ≡ forward Euler | exact FOH discretisation |
| Order | exact (it *is* the model) | $O(h^4)$ global | $O(h)$ global | exact for FOH inputs |
| Step | $\Delta t = 1$ (default) | $10^{-3}$ s | $10^{-4}$ s (control $10^{-3}$ s) | $0.01$ s |
| Samples | 150 | 10001 | 9000 | 200 |
| Dependencies | none | none | none | Eigen 3 |
| Reference agreement | exact recurrence | ≤ 2 × 10⁻⁷ | < 5 × 10⁻¹⁰ | ≈ 5 × 10⁻⁹ |

Each core's `tools/smoke_test.cpp` asserts these numbers, and
`tests/test_examples.py` re-runs all four through the shared adapters — so a
change in any algorithm above shows up as a failing `ctest`.
