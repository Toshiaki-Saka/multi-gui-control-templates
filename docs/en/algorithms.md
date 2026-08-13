# Algorithms

日本語版: [../ja/algorithms.md](../ja/algorithms.md)

Detailed documentation of what each core actually computes: plant models,
controllers, discretisation, integrators, error metrics, and the reasoning
behind each numerical choice. Every formula here corresponds to code in
`examples/<name>/core/src/`.

- [0. Common conventions](#0-common-conventions)
- [1. `pid` — PID attitude control](#1-pid--pid-attitude-control)
- [2. `mass_spring_damper` — forced response (RK4)](#2-mass_spring_damper--forced-response-rk4)
- [3. `pi_path_tracking` — PI path following](#3-pi_path_tracking--pi-path-following)
- [4. `two_dof` — 2-DOF control vs PID](#4-two_dof--2-dof-control-vs-pid)
- [5. Method comparison](#5-method-comparison)

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

## 1. `pid` — PID attitude control

Source: `examples/pid/core/src/pid_core.cpp`.
Port of `pid_advanced_simulation.py`.

### 1.1 Loop structure

The demo is deliberately the simplest closed loop that still shows P, I and D
behaviour: the "plant" is a pure accumulator, so the controller output is added
straight to the state.

```
        θ_goal ──▶( Σ )──e──▶[ PID ]──m──▶( Σ )──▶ θ
                    ▲ −                     ▲ −
                    │                       │
                    └──────── θ ◀───────────┘   offset
```

### 1.2 Control law

At step $n$ (with $`e_{-1} = 0`$, $`\Sigma_{-1} = 0`$):

$$e_n = \theta_{\rm goal} - \theta_n$$

$$\Sigma_n = \Sigma_{n-1} + e_n\,\Delta t$$

$$d_n = \frac{e_n - e_{n-1}}{\Delta t}$$

$$m_n^{\rm raw} = k_p\,e_n + k_i\,\Sigma_n + k_d\,d_n$$

```math
m_n = \begin{cases}
\mathrm{sat}(m_n^{\rm raw}, \pm M) & M > 0 \\
m_n^{\rm raw} & M = 0\ (\text{disabled})
\end{cases}
```

$$\theta_{n+1} = \theta_n + m_n - \mathrm{offset}$$

The integral is then clamped for the **next** iteration:

$$\Sigma_n \leftarrow \mathrm{sat}(\Sigma_n, \pm S) \quad (S > 0)$$

where $M$ is `output_clamp` and $S$ is `integral_clamp`.

### 1.3 Exact execution order

The ordering matters and is not the textbook one; it reproduces the reference
script step for step:

```text
for n = 1 .. N-1:
    e      = θ_goal − θ                   # error from the CURRENT state
    Σ     += e · dt                       # integrate BEFORE the clamp
    d      = (e − e_prev) / dt
    m_raw  = kp·e + ki·Σ + kd·d
    m      = clamp(m_raw, ±output_clamp)  # actuator saturation
    θ     += m                            # plant update: pure accumulator
    θ     −= offset                       # constant disturbance / bias
    Σ      = clamp(Σ, ±integral_clamp)    # anti-windup, applied AFTER use
    e_prev = e
    t[n]   = n · dt ;  θ[n] = θ
```

Two consequences worth knowing when reading the plots:

- **Anti-windup acts with one step of delay.** $`\Sigma_n`$ is used inside
  $`m_n`$ before being clamped, so the clamp limits the *stored* integrator, not
  the current output. The output clamp is what bounds the actuator immediately.
- **`offset` is applied unconditionally**, after the control update. It models a
  constant leak/bias that the integral term has to fight; with $k_i = 0$ the
  loop settles at a permanent offset error.

### 1.4 Role of `dt`

$\Delta t$ scales the integral and derivative terms
($`\Sigma`$ accumulates $`e\,\Delta t`$; $d$ divides by $\Delta t$), but the
plant update $`\theta \mathrel{+}= m`$ is **not** scaled. The plant is
"one control period per sample" by construction. With the default
$\Delta t = 1$ the core reproduces the reference recurrence exactly; changing
$\Delta t$ re-tunes I and D relative to P, which is exactly what the GUI slider
is for.

### 1.5 Time axis and outputs

Index 0 holds the initial state, so the arrays have exactly `time_length`
samples:

$$t_n = n\,\Delta t,\qquad \theta_0 = \theta_{\rm start},\qquad n = 0,\dots,N-1$$

Convenience metrics computed over $\theta$: `final_theta` (last sample),
`max_theta`, `min_theta` — enough to read off overshoot and steady-state error
without copying the array.

### 1.6 Validity

`pid_core_simulate()` returns `NULL` unless all of
$`\theta_{\rm start},\theta_{\rm goal},\mathrm{offset},k_p,k_i,k_d`$ are finite,
$\Delta t > 0$, both clamps are finite and $\ge 0$, and
$`\texttt{time\_length} \ge 2`$.

### 1.7 Defaults and behaviour

| Parameter | Default | Note |
|-----------|---------|------|
| $`\theta_{\rm start}`$ | 0 | |
| $`\theta_{\rm goal}`$ | 90 | |
| `offset` | 0 | disabled |
| `time_length` | 150 | samples |
| $k_p$ | 0.10 | |
| $k_i$ | 0.01 | |
| $k_d$ | 0.0 | |
| $\Delta t$ | 1.0 | |
| `integral_clamp` | 0 | disabled |
| `output_clamp` | 0 | disabled |

The defaults give a gentle, well-damped rise. The Python reference uses the more
aggressive $k_p = 0.10$, $k_i = 0.5$, $k_d = 0.5$, which overshoots to
$\theta \approx 135$ before settling at 90 — that is the response in the README
screenshot and the one the smoke test pins.

Because the plant is an accumulator (a discrete integrator), the loop is a
first-order system in $k_p$: the pole is at $1 - k_p$ per step, so
$0 < k_p < 2$ is the stable range with $k_i = k_d = 0$, and $k_p = 1$ is
dead-beat. Adding $k_i$ makes the loop second-order and introduces the
overshoot seen with the reference gains.

---

## 2. `mass_spring_damper` — forced response (RK4)

Source: `examples/mass_spring_damper/core/src/msd_core.cpp`.
Port of `mass_spring_damper_forced_response.py` (`scipy.integrate.odeint`).
Design notes: [../examples/mass_spring_damper/architecture.md](../examples/mass_spring_damper/architecture.md).

### 2.1 Plant

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

### 2.2 Derived quantities

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

### 2.3 Integrator — classical vector RK4

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

### 2.4 Time grid

$$t_k = k\,\Delta t,\qquad N = \left\lfloor \frac{t_{\rm stop} + \Delta t}{\Delta t} - 10^{-12}\right\rfloor + 1$$

matching `np.arange(0.0, stop + dt, dt)`, which yields one sample *past*
`stop`. Defaults $\Delta t = 10^{-3}$ s, $t_{\rm stop} = 10$ s give
$N = 10001$. The core rejects $N < 2$.

The force channel is stored alongside the states, sampled at the same grid
points ($`f_k = F\sin(\omega t_k)`$), so a frontend can overlay excitation and
response without recomputing anything.

### 2.5 Default case sweep

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

### 2.6 Metrics

`final_position`, `final_velocity`, `max_abs_position`, `max_abs_velocity`,
computed in the same pass as the integration.

### 2.7 References

- Den Hartog, J.P. (1985). *Mechanical Vibrations* (4th ed.). Dover.
- Inman, D.J. (2014). *Engineering Vibration* (4th ed.). Pearson.
- Press, W.H. et al. (2007). *Numerical Recipes in C++* (3rd ed.), §17.1 — Runge-Kutta.

---

## 3. `pi_path_tracking` — PI path following

Source: `examples/pi_path_tracking/core/src/track_core.cpp`.
Port of `planar_path_tracking_pi_tuned.py`.
C ABI details: [../examples/pi_path_tracking/api.md](../examples/pi_path_tracking/api.md).

### 3.1 States

| Symbol | Unit | Meaning |
|--------|------|---------|
| $u$ | m/s | body-frame longitudinal velocity |
| $v$ | m/s | body-frame lateral velocity (positive left) |
| $r$ | rad/s | yaw rate (positive counter-clockwise) |
| $x, y$ | m | global position |
| $\psi$ | rad | heading measured from $+X$ |

### 3.2 Plant — 3-DOF planar vehicle

$$\dot u = \frac{f_x}{m} + r\,v, \qquad \dot v = \frac{f_y}{m} - r\,u, \qquad \dot r = \frac{N}{I_{zz}}$$

$$\dot x = u\cos\psi - v\sin\psi, \qquad \dot y = u\sin\psi + v\cos\psi, \qquad \dot\psi = r$$

The $r v$ and $r u$ terms are the Coriolis accelerations that appear because the
momentum equations are written in the rotating body frame.

Lateral force from a linear slip model:

$$\beta = \arctan\frac{v}{u}, \qquad f_y = -C\,\beta$$

with $C$ the cornering power [N/rad]. Note the one-step lag in the
implementation: $\beta$ is recomputed at the **end** of each step, so the
$`f_y`$ used within step $n$ is built from the slip angle of step $n-1$
($`\beta_0 = 0`$). This is deliberate — it mirrors the reference script.

Defaults: $m = 0.1$ kg, $I_{zz} = 1.0$ kg·m², $C = 20$ N/rad.

### 3.3 Reference path

Three segments resampled at constant arc length $\Delta s$:

1. **Straight along $+X$**, $s \in [0, L_1)$: $(s,\ 0)$, $`\psi_{\rm ref} = 0`$.
2. **90° left arc** of radius $R$ centred at $(L_1, R)$, $\theta \in [0, \pi/2)$
   with $\Delta\theta = \Delta s / R$:

   $$x = L_1 + R\sin\theta, \qquad y = R(1-\cos\theta), \qquad \psi_{\rm ref} = \theta$$

3. **Straight along $+Y$** from the arc endpoint, $s \in [0, L_2)$:
   $(L_1+R,\ R + s)$, $`\psi_{\rm ref} = \pi/2`$.

Point counts follow `np.arange(0, L, ds)` semantics (upper bound excluded):

$$N_{\rm seg} = \left\lceil \frac{L}{\Delta s} - 10^{-12} \right\rceil$$

Defaults $L_1 = 0.30$ m, $R = 0.20$ m, $L_2 = 0.30$ m, $\Delta s = 0.002$ m give
458 points. The path is exposed separately through
`track_core_make_reference()` so a frontend can draw it without running a
simulation.

### 3.4 Nearest point and look-ahead

A **windowed** linear search, seeded with the previous index, keeps the cost
$O(1)$ per step and prevents the vehicle from latching onto a point behind it:

$$k^\* = \arg\min_{k\,\in\,[\max(0,\,k_{\rm prev}-5),\ \min(N_{\rm ref},\,k_{\rm prev}+120))} \bigl\lVert (x,y) - (x_{\rm ref}[k],\,y_{\rm ref}[k]) \bigr\rVert_2$$

The backward slack of 5 points tolerates small retreats; the forward window of
120 points ($0.24$ m at the default $\Delta s$) bounds how far the vehicle may
advance in one step.

The control target is a **look-ahead** point:

$$k_{\rm la} = \min\bigl(k^\* + L_{\rm la},\ N_{\rm ref}-1\bigr), \qquad L_{\rm la} = 60\ \text{points} = 0.12\ \text{m}$$

Look-ahead is what turns a pure position error into an anticipatory steering
command; without it, the $-k_{y,p}e_y$ term alone oscillates on the arc.

### 3.5 Error coordinates

With $`\delta x = x - x_{\rm ref}[k_{\rm la}]`$,
$`\delta y = y - y_{\rm ref}[k_{\rm la}]`$ and
$`\psi_r = \psi_{\rm ref}[k_{\rm la}]`$:

$$e_y = -\sin\psi_r\ \delta x + \cos\psi_r\ \delta y$$

$$e_\psi = \mathrm{wrap}_{(-\pi,\pi]}\bigl(\psi_r - \psi\bigr)$$

$`e_y`$ is the projection of the position error onto the reference normal
$(-\sin\psi_r, \cos\psi_r)$ — positive when the vehicle is to the left of the
path. The wrap is

$$\mathrm{wrap}_{(-\pi,\pi]}(\alpha) = \bigl((\alpha + \pi) \bmod 2\pi\bigr) - \pi$$

implemented with `std::fmod` plus a correction for the negative-remainder case,
so a heading error never jumps by $2\pi$ when the reference crosses $\pm\pi$.

### 3.6 Controllers

The controller runs on its own period $t_c$, i.e. every
$`\lfloor t_c/h \rfloor = 10`$ integration steps; between updates $f_x$ and $N$
are held constant (zero-order hold on the actuators). The counter starts at
zero and is tested with `>=`, so the **first** update happens at step 10, not
step 0: for the first millisecond the vehicle coasts with $f_x = N = 0$.

**Longitudinal PI** (gains hard-coded to match the reference):

$$e_{\rm spd} = v_{\rm target} - \sqrt{u^2+v^2}, \qquad \sigma_{\rm spd} \mathrel{+}= e_{\rm spd}\,t_c$$

$$f_x = \mathrm{sat}\bigl(100\,e_{\rm spd} + 0.1\,\sigma_{\rm spd},\ \pm f_{x,\rm lim}\bigr)$$

**Lateral + yaw PI with rate damping**, integrators clamped for anti-windup:

$$\sigma_y = \mathrm{sat}\bigl(\sigma_y + e_y t_c,\ \pm\sigma_{\rm lim}\bigr), \qquad \sigma_\psi = \mathrm{sat}\bigl(\sigma_\psi + e_\psi t_c,\ \pm\sigma_{\rm lim}\bigr)$$

$$N_{\rm raw} = -k_{y,p}e_y - k_{y,i}\sigma_y + k_{\psi,p}e_\psi + k_{\psi,i}\sigma_\psi - k_r\,r$$

$$N = \mathrm{sat}(N_{\rm raw},\ \pm N_{\rm lim})$$

Sign reading: $`-k_{y,p}e_y`$ yaws the vehicle back toward the path,
$`+k_{\psi,p}e_\psi`$ aligns the heading with the reference tangent, and
$`-k_r r`$ is a yaw-rate damper that suppresses the oscillation the first two
terms would otherwise excite on the arc.

| Gain | Default | Role |
|------|---------|------|
| $`k_{y,p}`$ | 400.0 | lateral proportional |
| $`k_{y,i}`$ | 0.0 | lateral integral (off by default) |
| $`k_{\psi,p}`$ | 200.0 | heading proportional |
| $`k_{\psi,i}`$ | 0.0 | heading integral (off by default) |
| $`k_r`$ | 20.0 | yaw-rate damping |
| $`N_{\rm lim}`$ | 500.0 N·m | moment saturation |
| $`f_{x,\rm lim}`$ | 5.0 N | force saturation |
| $`\sigma_{\rm lim}`$ | 0.2 | integrator clamp |
| $`L_{\rm la}`$ | 60 pts | look-ahead |
| $h$ / $t_c$ / $T$ | 10⁻⁴ / 10⁻³ / 0.90 s | step / control period / horizon |

Initial condition: on-path speed $u = v_{\rm target}$, offset
$y_0 = -0.03$ m and heading error $\psi_0 = 3°$, so the run starts with a
non-trivial transient to reject.

### 3.7 Integration — per-state scalar RK4 (and why it is Euler)

Each of the six states is advanced by its own scalar RK4 call, with every
*other* state frozen at its start-of-step value:

$$k_1 = h f(q_n),\quad k_2 = h f(q_n + \tfrac{k_1}{2}),\quad k_3 = h f(q_n + \tfrac{k_2}{2}),\quad k_4 = h f(q_n + k_3)$$

$$q_{n+1} = q_n + \frac{k_1 + 2k_2 + 2k_3 + k_4}{6}$$

But **no derivative in this model reads its own integrated variable**:

| State $q$ | Derivative $`\dot q`$ | Self-dependent? |
|-----------|----------------------|-----------------|
| $u$ | $`f_x/m + r_{\rm old}\,v_{\rm old}`$ | no |
| $v$ | $`f_y/m - r_{\rm old}\,u_{\rm old}`$ | no |
| $r$ | $`N/I_{zz}`$ | no |
| $x$ | $`u_{\rm old}\cos\psi_{\rm old} - v_{\rm old}\sin\psi_{\rm old}`$ | no |
| $y$ | $`u_{\rm old}\sin\psi_{\rm old} + v_{\rm old}\cos\psi_{\rm old}`$ | no |
| $\psi$ | $`r_{\rm old}`$ | no |

Since $`f(q_n + \alpha k;\,\mathbf{p}) = f(q_n;\,\mathbf{p})`$ for every
$\alpha$, $f$ is constant across the sub-steps,
$`k_1 = k_2 = k_3 = k_4 = h f(q_n)`$, and the combination collapses to

$$q_{n+1} = q_n + h\,f(q_n)$$

— plain **forward Euler**, with $O(h)$ global error rather than $O(h^4)$. The
step $h = 10^{-4}$ s is small enough that this is invisible (deviation from the
reference below $5\times10^{-10}$ m).

The RK4 scaffolding is kept for two reasons: bit-exact reproducibility of the
reference (the same floating-point operations in the same order), and
future-proofing — a nonlinear tyre model that reads $v$ inside $\dot v$ would
make the sub-steps non-trivial and the shell would then earn its accuracy
without a structural rewrite.

### 3.8 Step ordering

```text
for n = 0 .. num_steps-1:
    k*        = nearest_index(x, y, from k_prev)      # windowed search
    k_la      = min(k* + lookahead, N_ref-1)
    (e_y, e_ψ) = errors at k_la
    f_y       = −C · β                                 # β from the PREVIOUS step
    if control_counter ≥ steps_per_tc:                 # every tc seconds
        update f_x, N  (PI + damping, saturated)
    log(t, x, y, ψ, u, v, r, β, e_y, e_ψ, N, f_x, ref[k*], path_err)
    integrate u, v, r, x, y, ψ   (frozen old values)
    t += h ;  β = atan2(v, u)                          # β for the NEXT step
```

Logging happens **before** integration, so sample $n$ reports the state at
$t = n h$ together with the command that was applied over $[nh, (n{+}1)h)$.

### 3.9 Metrics

`path_err` is measured against the **nearest** point (not the look-ahead one),
so it is a true geometric distance:

$$\mathrm{path\_err}_n = \bigl\lVert (x_n,y_n) - (x_{\rm ref}[k^\*], y_{\rm ref}[k^\*]) \bigr\rVert_2$$

Aggregates: RMS and max of `path_err`, RMS and max-abs of $`e_y`$ and
$`e_\psi`$, and max-abs of $N$. With the defaults these are pinned by the smoke
test at `path_error_rms` = 0.013762 m, `path_error_max` = 0.030000 m,
`ey_rms` = 0.020243 m, `ey_max` = 0.032639 m, `epsi_rms` = 0.300645 rad,
`epsi_max` = 0.579404 rad, `max|N|` = 28.495415 N·m.

Note that $`e_\psi`$ is large in RMS because it is measured against the
look-ahead tangent: on the arc the reference heading 0.12 m ahead differs from
the current heading by construction, so a nonzero steady $`e_\psi`$ is the
correct tracking behaviour, not an error.

---

## 4. `two_dof` — 2-DOF control vs PID

Source: `examples/two_dof/core/src/{tf,simulate,tdof_core}.cpp`.
Port of the python-control reference.

This core answers one question: how much of PID's step-response overshoot comes
from the *reference path* rather than the feedback loop? It runs the same closed
loop twice — once with the raw step, once with a pre-filtered step.

### 4.1 Systems

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

### 4.2 Polynomial algebra

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

### 4.3 Controllable canonical form

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

### 4.4 First-order-hold discretisation

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

### 4.5 Time grid, scaling and reuse

$$t_i = i\,\Delta t, \qquad N = \left\lceil \frac{t_{\rm end}}{\Delta t} - 10^{-12} \right\rceil$$

matching `np.arange(0, t_end, dt)`; defaults $t_{\rm end} = 2$ s,
$\Delta t = 0.01$ s give $N = 200$. All signals are stored **normalised** to a
unit step; the `copy_r`, `copy_z`, `copy_y_pid`, `copy_y_2dof` accessors
multiply by `ref` on the way out, so changing the reference amplitude in a GUI
costs no recomputation. `copy_time` is never scaled.

`Gyz` is converted to state space once and the realisation is reused for both
$`y_{\rm pid}`$ and $`y_{\rm 2dof}`$; only $K_2$ needs its own conversion.
Anything Eigen throws is caught at the ABI boundary and reported as `NULL`.

### 4.6 Reading the result

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
