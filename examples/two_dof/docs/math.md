# Mathematical Background

This document provides the full mathematical derivation of the control systems
implemented in `tdof_core`.

> **Notation.** $s$ is the Laplace variable. Polynomial coefficients are listed
> highest-power-first throughout (matching the NumPy / python-control convention).

---

## 1. Plant Model

The plant is a **mass-spring-damper** second-order system:

$$P(s) = \frac{Y(s)}{U(s)} = \frac{1}{ms^2 + cs + k}$$

| Parameter | Symbol | Default | Unit |
|-----------|--------|---------|------|
| Mass | $m$ | $0.01$ | kg |
| Viscous damping | $c$ | $0.015$ | N·s/m |
| Spring stiffness | $k$ | $1.0$ | N/m |

The natural frequency and damping ratio are:

$$\omega_n = \sqrt{\frac{k}{m}}, \qquad \zeta = \frac{c}{2\sqrt{mk}}$$

With the default parameters: $\omega_n \approx 10\ \text{rad/s}$,
$\zeta = 0.015 / (2\sqrt{0.01 \cdot 1}) = 0.075$ (lightly damped).

---

## 2. PID Controller

A parallel-form PID with derivative in the forward path:

$$K_1(s) = k_p + \frac{k_i}{s} + k_d s = \frac{k_d s^2 + k_p s + k_i}{s}$$

| Parameter | Symbol | Default |
|-----------|--------|---------|
| Proportional gain | $k_p$ | $2.0$ |
| Integral gain | $k_i$ | $10.0$ |
| Derivative gain | $k_d$ | $0.1$ |

The numerator polynomial is $k_d s^2 + k_p s + k_i$ (degree 2);
the denominator is $s$ (pure integrator, degree 1), making $K_1$ improper.

---

## 3. Reference Pre-filter (2-DOF)

The reference pre-filter $K_2$ is obtained by forming the ratio of
the **PI part** of the PID numerator to the **full PID numerator**:

$$K_2(s) = \frac{k_p s + k_i}{k_d s^2 + k_p s + k_i}$$

This is a proper, stable second-order low-pass filter.
Its DC gain is $K_2(0) = k_i / k_i = 1$, so the steady-state value of the
filtered reference equals the original reference.

**Interpretation.**
The zeros of $K_1$ are the roots of $k_d s^2 + k_p s + k_i = 0$.
$K_2$ places those same roots in its denominator, creating a stable
denominator polynomial. Its numerator $k_p s + k_i$ removes the
aggressive derivative component from the reference path while preserving
the steady-state gain.

---

## 4. Open-loop and Closed-loop Transfer Functions

### 4.1 Open Loop

The open-loop transfer function (plant × PID) is:

$$L(s) = P(s)\,K_1(s)
= \frac{1}{ms^2+cs+k} \cdot \frac{k_d s^2 + k_p s + k_i}{s}
= \frac{k_d s^2 + k_p s + k_i}{ms^3 + cs^2 + ks}$$

### 4.2 Unity-feedback Closed Loop

Applying unity negative feedback:

$$G_{yz}(s) = \frac{L(s)}{1 + L(s)}
= \frac{k_d s^2 + k_p s + k_i}
       {ms^3 + cs^2 + ks + k_d s^2 + k_p s + k_i}$$

$$\boxed{G_{yz}(s) = \frac{k_d s^2 + k_p s + k_i}
       {ms^3 + (c+k_d)\,s^2 + (k+k_p)\,s + k_i}}$$

With default parameters:

$$G_{yz}(s) = \frac{0.1\,s^2 + 2\,s + 10}
       {0.01\,s^3 + 0.115\,s^2 + 3\,s + 10}$$

This is a **third-order** closed-loop system.

### 4.3 Polynomial Arithmetic in Code

The implementation realises the above operations via two primitives:

**Polynomial multiplication** (`poly_mul`):
For polynomials $A(s) = \sum_{i} a_i s^{n-i}$ and $B(s) = \sum_{j} b_j s^{m-j}$,

$$[A \cdot B]_k = \sum_{i+j=k} a_i\, b_j$$

**Polynomial addition** (`poly_add`):
Aligned at the constant term (lowest power), zero-padded on the left:

$$[A + B]_k = a_{k - (n_A - n_B)} + b_k \quad \text{(after left-padding the shorter operand)}$$

**Series connection** (`series`):
$$\text{series}(A, B) = \frac{A_\mathrm{num} \cdot B_\mathrm{num}}{A_\mathrm{den} \cdot B_\mathrm{den}}$$

**Unity feedback** (`feedback_unity`):
$$\text{feedback}(G, 1) = \frac{G_\mathrm{num}}{G_\mathrm{den} + G_\mathrm{num}}$$

---

## 5. Two Control Architectures

### 5.1 1-DOF PID

The reference step $r(t) = 1$ is fed directly into the closed loop:

$$Y_{\text{pid}}(s) = G_{yz}(s)\,R(s)$$

### 5.2 2-DOF-like

The reference is first pre-filtered by $K_2$, then passed to the same
closed loop:

$$Z(s) = K_2(s)\,R(s)$$
$$Y_{\text{2dof}}(s) = G_{yz}(s)\,Z(s) = G_{yz}(s)\,K_2(s)\,R(s)$$

The combined transfer function from $R$ to $Y_{\text{2dof}}$ is:

$$\frac{Y_{\text{2dof}}(s)}{R(s)} = G_{yz}(s)\,K_2(s)
= \frac{(k_d s^2 + k_p s + k_i)(k_p s + k_i)}
       {[ms^3+(c+k_d)s^2+(k+k_p)s+k_i]\,(k_d s^2+k_p s+k_i)}$$

The cancellation of the common factor $k_d s^2 + k_p s + k_i$ between
the numerator and the denominator bracket holds only when those roots are
identical; in practice this cancellation is exact (same polynomial)
and the effective reference-to-output transfer function reduces to:

$$\frac{Y_{\text{2dof}}(s)}{R(s)} = \frac{k_p s + k_i}
       {ms^3+(c+k_d)s^2+(k+k_p)s+k_i}$$

Note that this has the **same denominator** as $G_{yz}$ (same poles),
but a first-order numerator instead of second-order — the derivative
mode no longer excites the reference path.

---

## 6. State-space Realisation

To simulate $G_{yz}$ (and $K_2$) in discrete time, the transfer functions
are first converted to a state-space representation.

### 6.1 Controllable Canonical Form

Given a proper transfer function (degree of numerator $\le$ degree of
denominator):

$$H(s) = \frac{b_0 s^n + b_1 s^{n-1} + \cdots + b_n}
              {s^n + a_1 s^{n-1} + \cdots + a_n}$$

(denominator normalised to monic by dividing both polynomials by the
leading denominator coefficient)

The **direct feedthrough** term is $D = b_0$.
Define the modified numerator coefficients $\tilde{b}_i = b_i - b_0 a_i$
for $i = 1, \ldots, n$ (these are the coefficients after removing the
improper part).

The controllable canonical form is:

```math
A = \begin{pmatrix}
0 & 1 & 0 & \cdots & 0 \\
0 & 0 & 1 & \cdots & 0 \\
\vdots & & & \ddots & \vdots \\
0 & 0 & 0 & \cdots & 1 \\
-a_n & -a_{n-1} & -a_{n-2} & \cdots & -a_1
\end{pmatrix}, \quad
B = \begin{pmatrix} 0 \\ 0 \\ \vdots \\ 0 \\ 1 \end{pmatrix}
```

$$C = \begin{pmatrix} \tilde{b}_n & \tilde{b}_{n-1} & \cdots & \tilde{b}_1 \end{pmatrix}, \quad
D = b_0$$

The state equations are:

$$\dot{x}(t) = A\,x(t) + B\,u(t), \qquad y(t) = C\,x(t) + D\,u(t)$$

For $G_{yz}$ with default parameters ($n = 3$, $D = 0$ since strictly proper):

```math
A = \begin{pmatrix} 0 & 1 & 0 \\ 0 & 0 & 1 \\ -1000 & -300 & -11.5 \end{pmatrix}, \quad
B = \begin{pmatrix} 0 \\ 0 \\ 1 \end{pmatrix}
```

$$C = \begin{pmatrix} 1000 & 200 & 10 \end{pmatrix}, \quad D = 0$$

(coefficients obtained by dividing numerator and denominator by $0.01$)

---

## 7. First-Order-Hold (FOH) Discretisation

### 7.1 Motivation

Zero-order hold (ZOH) assumes $u(t) = u[k]$ constant within each interval
$[t_k, t_{k+1})$. For a smoothly varying input such as $z(t)$ (the filtered
reference), ZOH introduces a visible reconstruction error.

**First-order hold** (FOH) assumes $u(t)$ varies **linearly** between samples:

$$u(t) = u[k] + \frac{u[k+1] - u[k]}{\Delta t}(t - t_k), \qquad t \in [t_k,\, t_{k+1}]$$

This is exactly the interpolation model used by `python-control.forced_response`
for a uniform time grid, ensuring the C++ output matches to $\sim 5\times10^{-9}$.

### 7.2 Exact Discrete Recurrence

Under the FOH assumption the exact solution of the linear ODE at the next
sample point is:

$$x[k+1] = A_d\,x[k] + B_{d0}\,u[k] + B_{d1}\,u[k+1]$$
$$y[k] = C\,x[k] + D\,u[k]$$

The discrete matrices $A_d$, $B_{d0}$, $B_{d1}$ are computed via a single
matrix exponential of an **augmented** $(n+2)\times(n+2)$ matrix.

### 7.3 Augmented Matrix Exponential

Define:

```math
M = \begin{pmatrix}
A\,\Delta t & B\,\Delta t & 0 \\
0_{1\times n} & 0 & 1 \\
0_{1\times n} & 0 & 0
\end{pmatrix} \in \mathbb{R}^{(n+2)\times(n+2)}
```

Then:

```math
e^M = \begin{pmatrix}
A_d & * & B_{d1} \\
0 & 1 & * \\
0 & 0 & 1
\end{pmatrix}
```

where $*$ denotes entries not used. Extract:

$$A_d = \left[e^M\right]_{0:n,\;0:n}$$

$$B_{d1} = \left[e^M\right]_{0:n,\;n+1}$$

$$B_{d0} = \left[e^M\right]_{0:n,\;n} - B_{d1}$$

### 7.4 Derivation

Starting from the continuous-time ODE with the FOH input:

$$\dot{x}(t) = A\,x(t) + B\left[u[k] + \frac{u[k+1]-u[k]}{\Delta t}(t-t_k)\right]$$

Let $\tau = t - t_k \in [0, \Delta t]$. The variation-of-constants formula gives:

$$x[k+1] = e^{A\Delta t}\,x[k]
+ \int_0^{\Delta t} e^{A(\Delta t - \tau)} B
  \left[u[k] + \frac{u[k+1]-u[k]}{\Delta t}\,\tau\right] d\tau$$

Separating the two integrals:

$$x[k+1] = \underbrace{e^{A\Delta t}}_{A_d}\,x[k]
+ \underbrace{\left[\int_0^{\Delta t} e^{A(\Delta t-\tau)}\,d\tau\right] B
  - \frac{1}{\Delta t}\int_0^{\Delta t}\tau\,e^{A(\Delta t-\tau)}\,d\tau\, B}_{B_{d0}}\,u[k]
+ \underbrace{\frac{1}{\Delta t}\int_0^{\Delta t}\tau\,e^{A(\Delta t-\tau)}\,d\tau\, B}_{B_{d1}}\,u[k+1]$$

The augmented matrix trick encodes both integrals in a single matrix
exponential, avoiding explicit numerical integration. This is numerically
stable and efficient via Eigen's `MatrixBase::exp()` (Padé approximant).

### 7.5 Special Case: Pure Gain ($n = 0$)

When the transfer function reduces to a constant $H(s) = D$:

$$y[k] = D\,u[k]$$

No state update is needed.

---

## 8. Default Parameters and Smoke-test Values

With the default configuration:

| Parameter | Value |
|-----------|-------|
| $m$ | $0.01$ |
| $c$ | $0.015$ |
| $k$ | $1.0$ |
| $k_p$ | $2.0$ |
| $k_i$ | $10.0$ |
| $k_d$ | $0.1$ |
| $\text{ref}$ | $10.0$ |
| $t_\text{end}$ | $2.0$ s |
| $\Delta t$ | $0.01$ s |

The closed-loop polynomials (highest-power-first) are:

$$G_{yz}(s): \quad \text{num} = [0.1,\ 2,\ 10], \quad \text{den} = [0.01,\ 0.115,\ 3,\ 10]$$

Simulation results ($N = 200$ samples, output scaled by $\text{ref} = 10$):

| Signal | Peak | Final value | Overshoot |
|--------|------|-------------|-----------|
| $y_\text{pid}$ | $11.397$ | $9.996$ | $14.0\%$ |
| $y_\text{2dof}$ | $12.049$ | $9.995$ | $20.5\%$ |

The 2-DOF output has a larger overshoot in the *output* response while
the commanded reference $z$ is smoother than $r$, demonstrating the
trade-off between reference-path aggressiveness and output transient.

---

## 9. Summary of Signal Flow

```
                ┌──────┐     z     ┌───────┐    y_2dof
  r ──────────▶│  K2  │──────────▶│  Gyz  │──────────▶
  │            └──────┘           └───────┘
  │                                    ▲
  └────────────────────────────────────┘  y_pid  (direct path)
```

Both `y_pid` and `y_2dof` share the same closed-loop dynamics $G_{yz}$;
only the input signal differs (`r` vs `z = K2 r`).
