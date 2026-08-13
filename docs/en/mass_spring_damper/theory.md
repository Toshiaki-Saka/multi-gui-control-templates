# mass_spring_damper — forced response (RK4)

Back to the [algorithms overview](../algorithms.md) · [C ABI](api.md) · [日本語](../../ja/mass_spring_damper/theory.md)


Source: `examples/mass_spring_damper/core/src/msd_core.cpp`.
Port of `mass_spring_damper_forced_response.py` (`scipy.integrate.odeint`).
Design notes: [architecture.md](architecture.md).

## 1. Plant

A mass $m$ tied to a wall by a spring $k$ and a viscous dashpot $c$, driven by
an external force $f(t)$:

```
        k          c
wall ──/\/\/──┬──[==]──┬── → x(t)
              │         │
              └── [m] ──┘
                    ↑
                  f(t)
```

Newton's second law along the direction of motion gives

$$m\,\ddot{x} + c\,\dot{x} + k\,x = f(t), \qquad f(t) = F\sin(\omega t)$$

| Symbol | Unit | Meaning |
|--------|------|---------|
| $x(t)$ | m | displacement from equilibrium |
| $`\dot x(t)`$ | m/s | velocity |
| $`\ddot x(t)`$ | m/s² | acceleration |
| $m$ | kg | mass ($m > 0$) |
| $c$ | N·s/m | viscous damping coefficient ($c \ge 0$) |
| $k$ | N/m | spring constant ($k \ge 0$) |
| $F$ | N | force amplitude |
| $\omega$ | rad/s | excitation angular frequency |

As a first-order system in $`\mathbf{z} = (x, v)^{\mathsf T}`$, $v = \dot x$:

```math
\begin{bmatrix} \dot{x} \\ \dot{v} \end{bmatrix}
= \begin{bmatrix} 0 & 1 \\ -k/m & -c/m \end{bmatrix}
  \begin{bmatrix} x \\ v \end{bmatrix}
+ \begin{bmatrix} 0 \\ F\sin(\omega t)/m \end{bmatrix},
\qquad x(0) = x_0,\ v(0) = v_0
```

## 2. Derived quantities

$$\omega_n = \sqrt{\frac{k}{m}}, \qquad \zeta = \frac{c}{2\sqrt{mk}} = \frac{c}{2m\omega_n}$$

$`\omega_n`$ is the frequency at which the undamped, unforced system oscillates.
The damping ratio classifies the free response:

| Condition | Regime | Free response |
|-----------|--------|---------------|
| $`0 \le \zeta < 1`$ | underdamped | oscillatory decay |
| $\zeta = 1$ | critically damped | fastest non-oscillatory decay |
| $\zeta > 1$ | overdamped | exponential decay (two real modes) |

In the underdamped case the oscillation happens at the damped natural frequency

$$\omega_d = \omega_n\sqrt{1-\zeta^2}, \qquad \zeta < 1$$

`msd_core_derived()` exposes $`\omega_n`$ and $\zeta$ and returns 0 for
$m \le 0$ or $k < 0$; when $mk = 0$ the damping ratio is reported as 0 rather
than dividing by zero.

The steady-state forced amplitude, with frequency ratio $r = \omega/\omega_n$:

$$X = \frac{F/k}{\sqrt{(1-r^2)^2 + (2\zeta r)^2}}, \qquad \phi = \arctan\frac{2\zeta r}{1-r^2}$$

which is why the "near natural frequency" case ($r \approx 0.98$, $\zeta = 0.11$)
grows so much larger than the others.

## 3. Integrator — classical vector RK4

Unlike the reference, which uses adaptive LSODA, the core uses **fixed-step
RK4** on the full state vector. With $`\mathbf{f}(t,\mathbf{z})`$ the right-hand
side above and $h = \Delta t$:

$$\mathbf{k}_1 = \mathbf{f}(t_n,\ \mathbf{z}_n)$$

$$\mathbf{k}_2 = \mathbf{f}\!\left(t_n + \tfrac{h}{2},\ \mathbf{z}_n + \tfrac{h}{2}\mathbf{k}_1\right)$$

$$\mathbf{k}_3 = \mathbf{f}\!\left(t_n + \tfrac{h}{2},\ \mathbf{z}_n + \tfrac{h}{2}\mathbf{k}_2\right)$$

$$\mathbf{k}_4 = \mathbf{f}\!\left(t_n + h,\ \mathbf{z}_n + h\,\mathbf{k}_3\right)$$

$$\mathbf{z}_{n+1} = \mathbf{z}_n + \frac{h}{6}\bigl(\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4\bigr)$$

The time argument is threaded explicitly through the derivative helper so that
$`\mathbf{k}_2, \mathbf{k}_3`$ evaluate the forcing at $t_n + h/2$ and
$`\mathbf{k}_4`$ at $t_n + h$. Sampling the force only at $t_n$ would silently
drop the method to second order for the forced part.

| Property | Value |
|----------|-------|
| Local truncation error | $O(h^5)$ |
| Global error | $O(h^4)$ |
| Deviation from `scipy.odeint` | ≤ 2 × 10⁻⁷ for the default cases at $h = 10^{-3}$ s |

Stability for the homogeneous part requires $`|h\lambda|`$ inside the RK4
stability region ($\approx 2.79$ on the negative real axis); with
$`\omega_n \approx 2.2`$ and $h = 10^{-3}$ s there are four orders of magnitude
of margin.

## 4. Time grid

$$t_k = k\,\Delta t,\qquad N = \left\lfloor \frac{t_{\rm stop} + \Delta t}{\Delta t} - 10^{-12}\right\rfloor + 1$$

matching `np.arange(0.0, stop + dt, dt)`, which yields one sample *past*
`stop`. Defaults $\Delta t = 10^{-3}$ s, $t_{\rm stop} = 10$ s give
$N = 10001$. The core rejects $N < 2$.

The force channel is stored alongside the states, sampled at the same grid
points ($`f_k = F\sin(\omega t_k)`$), so a frontend can overlay excitation and
response without recomputing anything.

## 5. Default case sweep

All cases share $m = 1$ kg, $F = 0.5$ N, $x_0 = v_0 = 0$:

| Name | $c$ | $k$ | $\omega$ | $`\omega_n`$ | $\zeta$ | Regime |
|------|-----|-----|----------|-------------|---------|--------|
| baseline | 2.0 | 5.0 | 2.0 | 2.236 | 0.447 | underdamped |
| low damping | 0.5 | 5.0 | 2.0 | 2.236 | 0.112 | lightly damped |
| high damping | 5.0 | 5.0 | 2.0 | 2.236 | 1.118 | overdamped |
| stiffer spring | 2.0 | 12.0 | 2.0 | 3.464 | 0.289 | higher $`\omega_n`$ |
| near natural frequency | 0.5 | 5.0 | 2.2 | 2.236 | 0.112 | near resonance |

The C++ core stores no case list — `default_cases()` lives in the Python
binding and the equivalent tables in the Qt/Avalonia view models, so each
frontend can add or edit cases freely.

Expected final values at $t = 10$ s, pinned by the smoke test:

| Name | $x(10)$ [m] | $v(10)$ [m/s] |
|------|-------------|---------------|
| baseline | −0.021155 | +0.238803 |
| low damping | +0.109906 | +0.709929 |
| high damping | −0.015682 | +0.094431 |
| stiffer spring | +0.035444 | +0.086453 |
| near natural frequency | +0.409217 | −0.121325 |

## 6. Metrics

`final_position`, `final_velocity`, `max_abs_position`, `max_abs_velocity`,
computed in the same pass as the integration.

## 7. References

- Den Hartog, J.P. (1985). *Mechanical Vibrations* (4th ed.). Dover.
- Inman, D.J. (2014). *Engineering Vibration* (4th ed.). Pearson.
- Press, W.H. et al. (2007). *Numerical Recipes in C++* (3rd ed.), §17.1 — Runge-Kutta.
