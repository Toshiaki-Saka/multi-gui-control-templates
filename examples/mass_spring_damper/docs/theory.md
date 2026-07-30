# Theory — Mass-Spring-Damper Forced Response

## 1. System model

A single-degree-of-freedom (1-DOF) mass-spring-damper system consists of a
mass $m$ connected to a fixed wall by a spring with constant $k$ and a viscous
dashpot with damping coefficient $c$. An external sinusoidal force $f(t)$ is
applied to the mass.

```
        k          c
wall ──/\/\/──┬──[==]──┬── → x(t)
              │         │
              └── [m] ──┘
                    ↑
                  f(t)
```

### 1.1 Equation of motion

Applying Newton's second law along the direction of motion gives:

$$m\ddot{x} + c\dot{x} + kx = f(t)$$

where

| Symbol | Unit | Description |
|--------|------|-------------|
| $x(t)$ | m | displacement from equilibrium |
| $\dot{x}(t)$ | m/s | velocity |
| $\ddot{x}(t)$ | m/s² | acceleration |
| $m$ | kg | mass ($m > 0$) |
| $c$ | N·s/m | viscous damping coefficient ($c \geq 0$) |
| $k$ | N/m | spring constant ($k \geq 0$) |
| $f(t)$ | N | external force |

### 1.2 External force

The external forcing is purely sinusoidal:

$$f(t) = F \sin(\omega t)$$

where $F$ [N] is the force amplitude and $\omega$ [rad/s] is the excitation
angular frequency.

---

## 2. Derived parameters

### 2.1 Natural angular frequency

$$\omega_n = \sqrt{\frac{k}{m}}$$

This is the frequency at which the undamped system ($c = 0$, $f(t) = 0$)
oscillates freely.

### 2.2 Damping ratio

$$\zeta = \frac{c}{2\sqrt{mk}} = \frac{c}{2m\omega_n}$$

The damping ratio classifies the free-response behaviour:

| Condition | Regime | Free response |
|-----------|--------|---------------|
| $0 \leq \zeta < 1$ | Underdamped | Oscillatory decay |
| $\zeta = 1$ | Critically damped | Fastest non-oscillatory decay |
| $\zeta > 1$ | Overdamped | Exponential decay (two real modes) |

### 2.3 Damped natural frequency (underdamped case)

$$\omega_d = \omega_n \sqrt{1 - \zeta^2}, \quad \zeta < 1$$

### 2.4 Steady-state amplitude

At steady state, the particular solution to the forced equation is:

$$x_p(t) = X \sin(\omega t - \phi)$$

where the amplitude $X$ and phase $\phi$ are:

$$X = \frac{F/k}{\sqrt{\left(1 - r^2\right)^2 + \left(2\zeta r\right)^2}}, \qquad r = \frac{\omega}{\omega_n}$$

$$\phi = \arctan\!\left(\frac{2\zeta r}{1 - r^2}\right)$$

When $\omega \approx \omega_n$ (frequency ratio $r \approx 1$) and $\zeta$ is
small, $X$ becomes large — this is **resonance**. The "near natural frequency"
default case ($\omega = 2.2\ \mathrm{rad/s}$, $\omega_n \approx 2.24\ \mathrm{rad/s}$)
demonstrates this effect.

---

## 3. State-space formulation

Defining the state vector $`\mathbf{z} = \begin{bmatrix} x \\ v \end{bmatrix}`$
where $v = \dot{x}$, the second-order ODE becomes a first-order system:

$$\dot{\mathbf{z}} = \mathbf{A}\mathbf{z} + \mathbf{b}(t)$$

```math
\mathbf{A} = \begin{bmatrix} 0 & 1 \\ -\dfrac{k}{m} & -\dfrac{c}{m} \end{bmatrix}, \qquad \mathbf{b}(t) = \begin{bmatrix} 0 \\ \dfrac{f(t)}{m} \end{bmatrix}
```

Expanded:

```math
\begin{bmatrix} \dot{x} \\ \dot{v} \end{bmatrix} = \begin{bmatrix} 0 & 1 \\ -k/m & -c/m \end{bmatrix} \begin{bmatrix} x \\ v \end{bmatrix} + \begin{bmatrix} 0 \\ F\sin(\omega t)/m \end{bmatrix}
```

Initial conditions: $x(0) = x_0$, $v(0) = v_0$.

---

## 4. Numerical integration — 4th-order Runge-Kutta (RK4)

### 4.1 Algorithm

Given state $`\mathbf{z}_n`$ at time $t_n$, the RK4 update to $`t_{n+1} = t_n + h`$ is:

$$\mathbf{k}_1 = h \cdot \mathbf{f}(t_n,\; \mathbf{z}_n)$$

$$\mathbf{k}_2 = h \cdot \mathbf{f}\!\left(t_n + \tfrac{h}{2},\; \mathbf{z}_n + \tfrac{\mathbf{k}_1}{2}\right)$$

$$\mathbf{k}_3 = h \cdot \mathbf{f}\!\left(t_n + \tfrac{h}{2},\; \mathbf{z}_n + \tfrac{\mathbf{k}_2}{2}\right)$$

$$\mathbf{k}_4 = h \cdot \mathbf{f}(t_n + h,\; \mathbf{z}_n + \mathbf{k}_3)$$

$$\mathbf{z}_{n+1} = \mathbf{z}_n + \frac{1}{6}\left(\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4\right)$$

where $\mathbf{f}(t, \mathbf{z}) = \mathbf{A}\mathbf{z} + \mathbf{b}(t)$ and
$h = \Delta t$ is the fixed time step.

### 4.2 Accuracy

| Property | Value |
|----------|-------|
| Local truncation error | $O(h^5)$ per step |
| Global error | $O(h^4)$ over the simulation horizon |
| Comparison with `scipy.odeint` (LSODA) | $\leq 2 \times 10^{-7}$ for default cases at $h = 10^{-3}$ s |

### 4.3 Time grid

The time axis follows NumPy's `arange(0.0, stop + dt, dt)` convention, which
produces one extra trailing sample beyond `stop`:

$$t_k = k \cdot \Delta t, \quad k = 0, 1, \ldots, N-1$$

$$N = \left\lfloor \frac{t_{\mathrm{stop}} + \Delta t}{\Delta t} - \varepsilon \right\rfloor + 1$$

where $\varepsilon \sim 10^{-12}$ absorbs floating-point rounding in the
multiplication. For the defaults ($\Delta t = 10^{-3}$ s, $t_{\mathrm{stop}} = 10$ s)
this gives $N = 10001$ samples.

---

## 5. Default test cases

All cases share $m = 1\ \mathrm{kg}$, $F = 0.5\ \mathrm{N}$, $x_0 = 0\ \mathrm{m}$, $v_0 = 0\ \mathrm{m/s}$.

| Name | $c$ | $k$ | $\omega_n$ [rad/s] | $\zeta$ | $\omega$ [rad/s] | Notes |
|------|-----|-----|--------------------|---------|-------------------|-------|
| baseline | 2.0 | 5.0 | 2.236 | 0.447 | 2.0 | Moderate damping |
| low damping | 0.5 | 5.0 | 2.236 | 0.112 | 2.0 | Lightly damped, oscillatory |
| high damping | 5.0 | 5.0 | 2.236 | 1.118 | 2.0 | Overdamped ($\zeta > 1$) |
| stiffer spring | 2.0 | 12.0 | 3.464 | 0.289 | 2.0 | Higher natural frequency |
| near natural frequency | 0.5 | 5.0 | 2.236 | 0.112 | 2.2 | $\omega \approx \omega_n$, near resonance |

Expected final values (at $t = 10\ \mathrm{s}$, from the smoke test):

| Name | $x(10)$ [m] | $v(10)$ [m/s] |
|------|------------|--------------|
| baseline | −0.021155 | +0.238803 |
| low damping | +0.109906 | +0.709929 |
| high damping | −0.015682 | +0.094431 |
| stiffer spring | +0.035444 | +0.086453 |
| near natural frequency | +0.409217 | −0.121325 |

---

## 6. References

- Den Hartog, J.P. (1985). *Mechanical Vibrations* (4th ed.). Dover.
- Inman, D.J. (2014). *Engineering Vibration* (4th ed.). Pearson.
- Press, W.H. et al. (2007). *Numerical Recipes in C++* (3rd ed.), §17.1 — Runge-Kutta.
