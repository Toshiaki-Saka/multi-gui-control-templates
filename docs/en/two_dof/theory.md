# two_dof — 2-DOF control vs PID

Back to the [algorithms overview](../algorithms.md) · [C ABI](api.md) · [日本語](../../ja/two_dof/theory.md)


Source: `examples/two_dof/core/src/{tf,simulate,tdof_core}.cpp`.
Port of the python-control reference.

This core answers one question: how much of PID's step-response overshoot comes
from the *reference path* rather than the feedback loop? It runs the same closed
loop twice — once with the raw step, once with a pre-filtered step.

## 1. Systems

$$P(s) = \frac{1}{ms^2 + cs + k}, \qquad K_1(s) = k_p + \frac{k_i}{s} + k_d s = \frac{k_d s^2 + k_p s + k_i}{s}$$

$$K_2(s) = \frac{k_p s + k_i}{k_d s^2 + k_p s + k_i}$$

| Parameter | Symbol | Default | Unit |
|-----------|--------|---------|------|
| mass | $m$ | 0.01 | kg |
| viscous damping | $c$ | 0.015 | N·s/m |
| spring stiffness | $k$ | 1.0 | N/m |
| proportional gain | $k_p$ | 2.0 | — |
| integral gain | $k_i$ | 10.0 | — |
| derivative gain | $k_d$ | 0.1 | — |

$K_1$ has a degree-2 numerator over a degree-1 denominator (a pure integrator),
so it is **improper** on its own — which is why it is never realised in state
space by itself, only in series with $P$.

$K_2$ is the reference pre-filter: it re-uses the PID numerator as its
denominator and drops the derivative term from the numerator, so $K_2(0) = 1$
(no steady-state change) while the reference no longer excites the derivative
mode. Put differently, the zeros of $K_1$ (the roots of
$`k_d s^2 + k_p s + k_i`$) become the poles of $K_2$.

Open loop:

$$L(s) = P(s)K_1(s) = \frac{k_d s^2 + k_p s + k_i}{m s^3 + c s^2 + k s}$$

Unity-feedback closed loop:

$$G_{yz}(s) = \frac{L}{1 + L} = \frac{k_d s^2 + k_p s + k_i}{m s^3 + (c + k_d)s^2 + (k + k_p)s + k_i}$$

— a **third-order** system. Two runs share $`G_{yz}`$ and differ only in the
input:

$$y_{\rm pid} = G_{yz}\,r, \qquad z = K_2\,r, \qquad y_{\rm 2dof} = G_{yz}\,z$$

The effective reference-to-output transfer function of the 2-DOF path is worth
writing out, because a common factor cancels exactly:

$$\frac{Y_{\rm 2dof}(s)}{R(s)} = G_{yz}(s)K_2(s) = \frac{(k_d s^2 + k_p s + k_i)(k_p s + k_i)}{\bigl[m s^3+(c+k_d)s^2+(k+k_p)s+k_i\bigr](k_d s^2+k_p s+k_i)}$$

$$\frac{Y_{\rm 2dof}(s)}{R(s)} = \frac{k_p s + k_i}{m s^3+(c+k_d)s^2+(k+k_p)s+k_i}$$

Same denominator as $`G_{yz}`$ — identical poles — but a first-order numerator
instead of second-order. That is the whole effect of the pre-filter: the
derivative mode no longer appears in the reference path, while the feedback
dynamics are untouched.

Signal flow:

```
                ┌──────┐     z     ┌───────┐    y_2dof
  r ──────────▶│  K2  │──────────▶│  Gyz  │──────────▶
  │            └──────┘           └───────┘
  │                                    ▲
  └────────────────────────────────────┘  y_pid  (direct path)
```

## 2. Polynomial algebra

Coefficients are stored highest-power-first (NumPy / python-control
convention). Four primitives in `tf.cpp`:

| Operation | Definition |
|-----------|------------|
| `poly_mul` | convolution: $`[A\cdot B]_k = \sum_{i+j=k} a_i b_j`$ |
| `poly_add` | aligned at the constant term; the shorter operand is zero-padded on the **left** |
| `series(A,B)` | $`(A_{\rm num}B_{\rm num}) / (A_{\rm den}B_{\rm den})`$ |
| `feedback_unity(G)` | $`G_{\rm num} / (G_{\rm den} + G_{\rm num})`$ |

`normalize_leading()` strips leading coefficients below $10^{-14}$ (keeping at
least one), which is what makes degenerate configurations behave: with
$k_d = 0$ the filter $K_2$ drops from second to first order instead of carrying
a zero leading term into the state-space conversion.

## 3. Controllable canonical form

For a proper $H(s) = \dfrac{b_0 s^n + \dots + b_n}{s^n + a_1 s^{n-1} + \dots + a_n}$
(after dividing both polynomials by the leading denominator coefficient), with
$D = b_0$ and $`\tilde b_i = b_i - b_0 a_i`$:

```math
A = \begin{pmatrix}
0 & 1 & 0 & \cdots & 0 \\
0 & 0 & 1 & \cdots & 0 \\
\vdots & & & \ddots & \vdots \\
0 & 0 & 0 & \cdots & 1 \\
-a_n & -a_{n-1} & -a_{n-2} & \cdots & -a_1
\end{pmatrix},\quad
B = \begin{pmatrix} 0 \\ \vdots \\ 0 \\ 1 \end{pmatrix},\quad
C = \begin{pmatrix} \tilde b_n & \tilde b_{n-1} & \cdots & \tilde b_1 \end{pmatrix}
```

The numerator is left-padded to length $n+1$ first, so strictly proper systems
simply get $b_0 = 0 \Rightarrow D = 0$. A zero-order system ($n = 0$) degenerates
to the pure gain $y = D u$, handled as a special case.

With the defaults, $`G_{yz}`$ is third order:

```math
A = \begin{pmatrix} 0 & 1 & 0 \\ 0 & 0 & 1 \\ -1000 & -300 & -11.5 \end{pmatrix},\quad
B = \begin{pmatrix} 0 \\ 0 \\ 1 \end{pmatrix},\quad
C = \begin{pmatrix} 1000 & 200 & 10 \end{pmatrix},\quad D = 0
```

## 4. First-order-hold discretisation

`python-control.forced_response` on a uniform grid assumes the input varies
**linearly** between samples, not piecewise-constant. Matching that is what
buys the ≈ 5 × 10⁻⁹ agreement, and it matters here because $z(t)$ (the filtered
reference) is smooth.

Under FOH the exact solution at the next sample is

$$x[k+1] = A_d x[k] + B_{d0} u[k] + B_{d1} u[k+1], \qquad y[k] = C x[k] + D u[k]$$

Starting from variation of constants with $\tau = t - t_k$:

$$x[k+1] = e^{A\Delta t}x[k] + \int_0^{\Delta t}\! e^{A(\Delta t - \tau)} B\left[u[k] + \frac{u[k+1]-u[k]}{\Delta t}\tau\right]\!d\tau$$

so that $A_d = e^{A\Delta t}$, $`B_{d1} = \frac{1}{\Delta t}\int_0^{\Delta t}\tau e^{A(\Delta t-\tau)}d\tau\,B`$
and $`B_{d0} = \left[\int_0^{\Delta t} e^{A(\Delta t-\tau)}d\tau\right]B - B_{d1}`$.
Both integrals are obtained from a single matrix exponential of an augmented
$(n+2)\times(n+2)$ matrix:

```math
M = \begin{pmatrix}
A\,\Delta t & B\,\Delta t & 0 \\
0 & 0 & 1 \\
0 & 0 & 0
\end{pmatrix},
\qquad
e^{M} = \begin{pmatrix}
A_d & * & B_{d1} \\
0 & 1 & * \\
0 & 0 & 1
\end{pmatrix}
```

with $`B_{d0} = [e^M]_{0:n,\,n} - B_{d1}`$. Eigen's `MatrixBase::exp()`
(scaling-and-squaring with a Padé approximant) evaluates it; there is no
explicit numerical quadrature and no per-step matrix exponential — $A_d$,
$`B_{d0}`$, $`B_{d1}`$ are computed once and reused for all $N$ steps.

The recursion starts from zero state, so $y[0] = D\,u[0]$ (zero for a strictly
proper system).

## 5. Time grid, scaling and reuse

$$t_i = i\,\Delta t, \qquad N = \left\lceil \frac{t_{\rm end}}{\Delta t} - 10^{-12} \right\rceil$$

matching `np.arange(0, t_end, dt)`; defaults $t_{\rm end} = 2$ s,
$\Delta t = 0.01$ s give $N = 200$. All signals are stored **normalised** to a
unit step; the `copy_r`, `copy_z`, `copy_y_pid`, `copy_y_2dof` accessors
multiply by `ref` on the way out, so changing the reference amplitude in a GUI
costs no recomputation. `copy_time` is never scaled.

`Gyz` is converted to state space once and the realisation is reused for both
$`y_{\rm pid}`$ and $`y_{\rm 2dof}`$; only $K_2$ needs its own conversion.
Anything Eigen throws is caught at the ABI boundary and reported as `NULL`.

## 6. Reading the result

Defaults $m = 0.01$, $c = 0.015$, $k = 1.0$, $k_p = 2$, $k_i = 10$, $k_d = 0.1$,
`ref` = 10:

$$G_{yz}(s) = \frac{0.1 s^2 + 2 s + 10}{0.01 s^3 + 0.115 s^2 + 3 s + 10}$$

which is what `tdof_core_get_tf(cfg, 3, …)` returns as the coefficient arrays
(highest power first):

```
num = [0.1,  2,     10]
den = [0.01, 0.115, 3, 10]
```

Simulation results ($N = 200$ samples, output scaled by `ref` = 10):

| Signal | Peak | Final | Overshoot |
|--------|------|-------|-----------|
| $`y_{\rm pid}`$ | 11.397 | 9.996 | 14.0 % |
| $`y_{\rm 2dof}`$ | 12.049 | 9.995 | 20.5 % |

The interesting part is that the pre-filtered run has the *larger* output
overshoot while its command signal $z$ is the smoother one. Feeding a smoothed
reference through the same loop is not automatically gentler on the output —
which is exactly the trade-off the demo exists to show. Context for reading it:
the plant is lightly damped ($\zeta = 0.075$, $`\omega_n = 10`$ rad/s) and with
the default gains $K_2$ has a double pole at $s = -10$, i.e. its corner
frequency sits right on the plant's natural frequency.
`tdof_core_get_tf()` lets a frontend print all four transfer functions
($P$, $K_1$, $K_2$, $`G_{yz}`$) so the pole/zero placement can be inspected
directly for any gain set.
