# Numerical Methods

## 1. Per-State Scalar RK4

The integrator uses **per-state scalar RK4** rather than a conventional vector RK4.
Each of the six state variables is advanced independently, with all *other* states
frozen at their values from the **beginning** of the step.

For a state $q$ with derivative $\dot{q} = f(q;\, \mathbf{p})$, where $\mathbf{p}$ denotes
the frozen values of the remaining states plus the current force inputs:

$$
k_1 = h\; f\!\left(q_n;\; \mathbf{p}\right)
$$

$$
k_2 = h\; f\!\left(q_n + \tfrac{k_1}{2};\; \mathbf{p}\right)
$$

$$
k_3 = h\; f\!\left(q_n + \tfrac{k_2}{2};\; \mathbf{p}\right)
$$

$$
k_4 = h\; f\!\left(q_n + k_3;\; \mathbf{p}\right)
$$

$$
q_{n+1} = q_n + \frac{k_1 + 2k_2 + 2k_3 + k_4}{6}
$$

This is *not* equivalent to a standard vector RK4: the cross-coupling terms
(e.g., $r v$ in $\dot{u}$, $r u$ in $\dot{v}$) are evaluated only at the start of the step,
and the four sub-steps do not see the intermediate updates of the coupled states.

---

## 2. Why All Derivatives Are Constant Within a Step

Inspecting each derivative function reveals that **none depends on its own integrated variable**:

| State $q$ | Derivative $\dot{q}$ | Self-dependent? |
|-----------|----------------------|----------------|
| $u$ | $f_x/m + r_{\rm old}\, v_{\rm old}$ | No |
| $v$ | $f_y/m - r_{\rm old}\, u_{\rm old}$ | No |
| $r$ | $N / I_{zz}$ | No |
| $x$ | $u_{\rm old}\cos\psi_{\rm old} - v_{\rm old}\sin\psi_{\rm old}$ | No |
| $y$ | $u_{\rm old}\sin\psi_{\rm old} + v_{\rm old}\cos\psi_{\rm old}$ | No |
| $\psi$ | $r_{\rm old}$ | No |

Because $f(q_n + \alpha k;\, \mathbf{p}) = f(q_n;\, \mathbf{p})$ for all $\alpha$, the four
sub-step slopes are identical:

$$
k_1 = k_2 = k_3 = k_4 = h\; f(q_n;\, \mathbf{p})
$$

Substituting into the RK4 combination formula:

$$
q_{n+1}
= q_n + \frac{k_1 + 2k_2 + 2k_3 + k_4}{6}
= q_n + \frac{6 \cdot h\; f}{6}
= q_n + h\; f(q_n;\, \mathbf{p})
$$

This is **forward Euler** with step $h = 10^{-4}$ s.

> **Consequence.** The local truncation error is $O(h^2)$ and the global error is $O(h)$,
> the same as first-order Euler — not the $O(h^4)$ / $O(h^5)$ one would normally expect from RK4.
> The step size $h = 10^{-4}$ s is chosen small enough that this does not matter in practice:
> the trajectory deviates from the Python reference by less than $5 \times 10^{-10}$ m.

---

## 3. Why the RK4 Scaffolding Is Preserved

The four-sub-step form is kept verbatim for two reasons:

1. **Bit-exact reproducibility.** The Python reference calls a generic scalar `rk4` helper
   for each state.  Replicating that call structure (even though it collapses to Euler)
   guarantees that floating-point rounding happens in exactly the same order,
   producing numerically identical output.

2. **Future-proofing.** A lateral-force model that reads $v$ inside $\dot{v}$
   (e.g., a nonlinear tyre) would make the sub-steps non-trivial; the RK4 shell
   would then improve accuracy without requiring a structural rewrite.

---

## 4. Reference Path Sampling

The Python reference builds each path segment with `np.arange(0.0, L, ds)`,
which generates points $0, ds, 2ds, \ldots$ up to but **not including** $L$.
The equivalent point count is:

$$
N = \left\lceil \frac{L}{\Delta s} - \varepsilon \right\rceil, \qquad \varepsilon = 10^{-12}
$$

The $\varepsilon$ guard prevents an extra point when $L$ is an exact multiple of $\Delta s$
(floating-point arithmetic can make $L / \Delta s$ slightly exceed the integer,
and `ceil` would then add a spurious endpoint).

The same formula applies to the arc segment with $L = \pi/2$ and $\Delta s = \Delta\theta = ds/R$.

---

## 5. Angle Wrapping

Heading error requires wrapping $(\psi_r - \psi)$ to $(-\pi, \pi]$:

$$
\mathrm{wrap}_{(-\pi,\pi]}(\alpha)
= \Bigl((\alpha + \pi) \bmod 2\pi\Bigr) - \pi
$$

implemented via `std::fmod` with a correction for the negative-remainder case.

---

## 6. Accuracy Summary

With default parameters the C++ core matches the Python reference to **all 6 printed decimal digits**
on every aggregate metric:

| Metric | Value |
|--------|-------|
| `path_error_rms` | 0.013762 m |
| `path_error_max` | 0.030000 m |
| `e_y_rms` | 0.020243 m |
| `e_y_max` | 0.032639 m |
| `e_psi_rms` | 0.300645 rad |
| `e_psi_max` | 0.579404 rad |
| `max\|N\|` | 28.495415 N·m |

The trajectory agreement is within $5 \times 10^{-10}$ m — pure floating-point noise.
