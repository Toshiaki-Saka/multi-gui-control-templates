# pid — PID attitude control

Back to the [algorithms overview](../algorithms.md) · [C ABI](api.md) · [日本語](../../ja/pid/theory.md)


Source: `examples/pid/core/src/pid_core.cpp`.
Port of `pid_advanced_simulation.py`.

## 1. Loop structure

The demo is deliberately the simplest closed loop that still shows P, I and D
behaviour: the "plant" is a pure accumulator, so the controller output is added
straight to the state.

```
        θ_goal ──▶( Σ )──e──▶[ PID ]──m──▶( Σ )──▶ θ
                    ▲ −                     ▲ −
                    │                       │
                    └──────── θ ◀───────────┘   offset
```

## 2. Control law

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

## 3. Exact execution order

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

## 4. Role of `dt`

$\Delta t$ scales the integral and derivative terms
($`\Sigma`$ accumulates $`e\,\Delta t`$; $d$ divides by $\Delta t$), but the
plant update $`\theta \mathrel{+}= m`$ is **not** scaled. The plant is
"one control period per sample" by construction. With the default
$\Delta t = 1$ the core reproduces the reference recurrence exactly; changing
$\Delta t$ re-tunes I and D relative to P, which is exactly what the GUI slider
is for.

## 5. Time axis and outputs

Index 0 holds the initial state, so the arrays have exactly `time_length`
samples:

$$t_n = n\,\Delta t,\qquad \theta_0 = \theta_{\rm start},\qquad n = 0,\dots,N-1$$

Convenience metrics computed over $\theta$: `final_theta` (last sample),
`max_theta`, `min_theta` — enough to read off overshoot and steady-state error
without copying the array.

## 6. Validity

`pid_core_simulate()` returns `NULL` unless all of
$`\theta_{\rm start},\theta_{\rm goal},\mathrm{offset},k_p,k_i,k_d`$ are finite,
$\Delta t > 0$, both clamps are finite and $\ge 0$, and
$`\texttt{time\_length} \ge 2`$.

## 7. Defaults and behaviour

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

The defaults still overshoot: measured from the app, the response peaks at
$\theta = 117.651$ (about 31 %) and settles at $\theta = 89.983$. That is the
state shown in the [screenshot](screenshot.png). The header comment in
`pid_core.h` calls this configuration "gentle, well-damped", which understates
it — the integral term alone is enough to make the loop second-order and
overshoot.

The Python reference uses the more aggressive $k_p = 0.10$, $k_i = 0.5$,
$k_d = 0.5$, which overshoots to $\theta \approx 135$ before settling at 90.
That is the response the smoke test pins.

Because the plant is an accumulator (a discrete integrator), the loop is a
first-order system in $k_p$: the pole is at $1 - k_p$ per step, so
$0 < k_p < 2$ is the stable range with $k_i = k_d = 0$, and $k_p = 1$ is
dead-beat. Adding $k_i$ makes the loop second-order and introduces the
overshoot seen with the reference gains.
